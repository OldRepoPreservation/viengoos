/* bucket-manage-mt.c - Manage RPCs on a bucket.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>

   This file is part of the GNU Hurd.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>

#include <l4.h>
#include <compiler.h>

#include "cap-server-intern.h"


/* When using propagation, the from thread ID returned can differ from
   the one we used for the closed receive.  */
#define l4_xreceive_timeout(from,timeout,fromp) \
  (l4_ipc (l4_nilthread, from, timeout, fromp))
#define l4_xreceive(from,fromp) \
  l4_xreceive_timeout (from, l4_timeouts (L4_ZERO_TIME, L4_NEVER), fromp)


/* FIXME: Throughout this file, for debugging, the behaviour could be
   relaxed to return errors to callers which would otherwise be
   ignored (due to malformed requests etc).  */


/* FIXME: This section contains a lot of random junk that maybe should
   be somewhere else (or not).  */

/* Cancel the pending RPC of the specified thread.  */
#define HURD_CAP_MSG_LABEL_CANCEL	0x100
#define HURD_CAP_MSG_LABEL_GET_ROOT	0x101

/* Some error for this.  */
#define ECAP_NOREPLY 0x10001
#define ECAP_DIED    0x10002


/* The msg labels of the reply from the worker to the manager.  */
#define _HURD_CAP_MSG_WORKER_ACCEPTED 0
#define _HURD_CAP_MSG_WORKER_REJECTED 1

struct worker_info
{
  /* The bucket.  */
  hurd_cap_bucket_t bucket;

  /* The manager thread.  */
  l4_thread_id_t manager_tid;

  /* The timeout for the worker thread as an L4 time period.  */
  l4_time_t timeout;
};


static void
__attribute__((always_inline))
reply_err (l4_thread_id_t to, error_t err)
{
#define HURD_L4_ERROR_LABEL ((uint16_t) INT16_MIN)
#define HURD_L4_ERROR_TAG ((HURD_L4_ERROR_LABEL << 16) & 1)
  l4_msg_tag_t tag = HURD_L4_ERROR_TAG;

  l4_set_msg_tag (tag);
  l4_load_mr (1, err);
  l4_reply (to);
}


/* Lookup the client with client ID CLIENT_ID and return it in
   *R_CLIENT with one reference for the entry in the bucket.  It is
   verified that the client is in fact the one with the task ID
   TASK_ID.  */
static error_t
__attribute__((always_inline))
lookup_client (hurd_cap_bucket_t bucket, _hurd_cap_client_id_t client_id,
	       hurd_task_id_t task_id, _hurd_cap_client_t *r_client)
{
  error_t err = 0;
  _hurd_cap_client_t *clientp;

  pthread_mutex_lock (&bucket->lock);
  /* Look up the client by its ID.  */
  clientp = hurd_table_lookup (&bucket->clients, client_id);
  if (!clientp || (*clientp)->dead || (*clientp)->task_id != task_id)
    err = ECAP_NOREPLY;
  else
    {
      (*clientp)->refs++;
      *r_client = *clientp;
    }
  pthread_mutex_unlock (&bucket->lock);

  return err;
}


/* Process the message CTX->MSG from thread CTX->FROM in worker thread
   CTX->WORKER of bucket CTX->BUCKET.  Return ECAP_NOREPLY if no reply
   should be sent.  Any other error will be replied to the user.  If 0
   is returned, CTX->MSG must contain the reply message.  */
static error_t
__attribute__((always_inline))
manage_demuxer (hurd_cap_rpc_context_t ctx, _hurd_cap_list_item_t worker)
{
  error_t err = 0;
  hurd_cap_bucket_t bucket = ctx->bucket;
  _hurd_cap_client_t client;
  hurd_cap_handle_t cap;
  hurd_cap_class_t cap_class;
  hurd_cap_obj_t obj;
  _hurd_cap_obj_entry_t obj_entry;
  struct _hurd_cap_list_item worker_client;
  struct _hurd_cap_list_item worker_class;
  struct _hurd_cap_list_item worker_obj;

  worker_client.thread = worker->thread;
  worker_client.tid = worker->tid;
  worker_client.next = NULL;
  worker_client.prevp = NULL;

  worker_class = worker_client;
  worker_obj = worker_client;

  if (l4_msg_label (ctx->msg) == HURD_CAP_MSG_LABEL_GET_ROOT)
    {
      /* This is the "get the root capability" RPC.  FIXME: Needs to
	 be implemented.  */
      return ENOSYS;
    }

  /* Every normal RPC must have at least one untyped word, which
     contains the client and capability ID.  Otherwise the message is
     malformed, and thus ignored.  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) < 1)
    return ECAP_NOREPLY;
  cap = l4_msg_word (ctx->msg, 0);

  err = lookup_client (bucket, _hurd_cap_client_id (cap),
		       ctx->sender, &client);
  if (err)
    return err;

  /* At this point, CLIENT_ID and CLIENT are valid, and we have one
     reference for the client.  */

  pthread_mutex_lock (&client->lock);
  /* First, we have to check if the class is inhibited, and if it is,
     we have to wait until it is uninhibited.  */
  if (EXPECT_FALSE (client->state == _HURD_CAP_STATE_BLACK))
    err = ECAP_NOREPLY;
  else if (EXPECT_FALSE (client->state != _HURD_CAP_STATE_GREEN))
    {
      pthread_mutex_unlock (&client->lock);
      pthread_mutex_lock (&bucket->client_cond_lock);
      pthread_mutex_lock (&client->lock);
      while (!err && client->state != _HURD_CAP_STATE_GREEN)
	{
	  if (client->state == _HURD_CAP_STATE_BLACK)
	    err = ECAP_NOREPLY;
	  else
	    {
	      pthread_mutex_unlock (&client->lock);
	      err = hurd_cond_wait (&bucket->client_cond,
				    &bucket->client_cond_lock);
	      pthread_mutex_lock (&client->lock);
	    }
	}
      pthread_mutex_unlock (&bucket->client_cond_lock);
    }
  if (err)
    {
      pthread_mutex_unlock (&client->lock);
      /* Either the client died, or we have been canceled.  */
      _hurd_cap_client_release (bucket, client->id);
      return err;
    }

  {
    _hurd_cap_obj_entry_t *entry;

    entry = (_hurd_cap_obj_entry_t *) hurd_table_lookup (&client->caps,
							 _hurd_cap_id (cap));
    if (!entry)
      err = ECAP_NOREPLY;
    else
      {
	obj_entry = *entry;

	if (EXPECT_FALSE (!obj_entry->external_refs))
	  err = ECAP_NOREPLY;
	else if (EXPECT_FALSE (obj_entry->dead))
	  err = ECAP_DIED;
	else
	  {
	    obj_entry->internal_refs++;
	    obj = obj_entry->cap_obj;
	  }
      }
  }
  if (err)
    {
      /* Either the capability ID is invalid, or it was revoked.  */
      pthread_mutex_unlock (&client->lock);
      _hurd_cap_client_release (bucket, client->id);
      return err;
    }

  /* At this point, CAP_ID, OBJ_ENTRY and OBJ are valid.  We have one
     internal reference for the capability entry.  */

  /* Add ourself to the pending_rpcs list of the client.  */
  _hurd_cap_list_item_add (&client->pending_rpcs, &worker_client);
  pthread_mutex_unlock (&client->lock);

  cap_class = obj->cap_class;

  pthread_mutex_lock (&cap_class->lock);
  /* First, we have to check if the class is inhibited, and if it is,
     we have to wait until it is uninhibited.  */
  while (!err && cap_class->state != _HURD_CAP_STATE_GREEN)
    err = hurd_cond_wait (&cap_class->cond, &cap_class->lock);
  if (err)
    {
      /* Canceled.  */
      pthread_mutex_unlock (&cap_class->lock);
      goto client_cleanup;
    }

  _hurd_cap_list_item_add (&cap_class->pending_rpcs, &worker_class);
  pthread_mutex_unlock (&cap_class->lock);


  pthread_mutex_lock (&obj->lock);
  /* First, we have to check if the object is inhibited, and if it is,
     we have to wait until it is uninhibited.  */
  if (obj->state != _HURD_CAP_STATE_GREEN)
    {
      pthread_mutex_unlock (&obj->lock);
      pthread_mutex_lock (&cap_class->obj_cond_lock);
      pthread_mutex_lock (&obj->lock);
      while (!err && obj->state != _HURD_CAP_STATE_GREEN)
	{
	  pthread_mutex_unlock (&obj->lock);
	  err = hurd_cond_wait (&cap_class->obj_cond,
				&cap_class->obj_cond_lock);
	  pthread_mutex_lock (&obj->lock);
	}
      pthread_mutex_unlock (&cap_class->obj_cond_lock);
    }
  if (err)
    {
      /* Canceled.  */
      pthread_mutex_unlock (&obj->lock);
      goto class_cleanup;
    }

  /* Now check if the client still has the capability, or if it was
     revoked.  */
  pthread_mutex_lock (&client->lock);
  if (obj_entry->dead)
    err = ECAP_DIED;
  pthread_mutex_unlock (&client->lock);
  if (err)
    {
      /* The capability was revoked in the meantime.  */
      pthread_mutex_unlock (&obj->lock);
      goto class_cleanup;
    }
  _hurd_cap_list_item_add (&obj->pending_rpcs, &worker_obj);

  /* At this point, we have looked up the capability, acquired an
     internal reference for its entry in the client table (which
     implicitely keeps a reference acquired for the object itself),
     acquired a reference for the capability client in the bucket, and
     have added an item to the pending_rpcs lists in the client, class
     and object.  The object is locked.  With all this, we can finally
     start to process the message for real.  */

  /* FIXME: Call the internal demuxer here, for things like reference
     counter modification, cap passing etc.  */

  /* Invoke the class-specific demuxer.  */
  ctx->client = client;
  ctx->obj = obj;
  err = (*cap_class->demuxer) (ctx);

  /* Clean up.  OBJ is still locked.  */
  _hurd_cap_list_item_remove (&worker_obj);
  _hurd_cap_obj_cond_check (obj);

  /* Instead releasing the lock for the object, we hold it until
     manage_demuxer_cleanup is called.  This is important, because the
     object must be locked until the reply message is sent.  Consider
     the impact of map items or string items.  FIXME: Alternatively,
     let the user set a flag if the object is locked upon return (and
     must be kept lock continuously until the reply is sent).  OTOH,
     releasing a lock just to take it again is also pretty useless.
     Needs performance measurements to make a good decision.  */
  hurd_cap_obj_ref (obj);

 class_cleanup:
  pthread_mutex_lock (&cap_class->lock);
  _hurd_cap_list_item_remove (&worker_class);
  _hurd_cap_class_cond_check (cap_class);
  pthread_mutex_unlock (&cap_class->lock);

 client_cleanup:
  pthread_mutex_lock (&client->lock);
  _hurd_cap_list_item_remove (&worker_client);
  _hurd_cap_client_cond_check (bucket, client);

  /* You are not allowed to revoke a capability while there are
     pending RPCs on it.  This is the reason why we know that there
     must be at least one extra internal reference.  FIXME: For
     cleanliness, this could still call some inline function that does
     the decrement.  The assert can be a hint to the compiler to
     optimize the inline function expansion anyway.  */
  assert (!obj_entry->dead);
  assert (obj_entry->internal_refs > 1);
  obj_entry->internal_refs--;
  pthread_mutex_unlock (&client->lock);

  _hurd_cap_client_release (bucket, client->id);

  return err;
}


static void
__attribute__((always_inline))
manage_demuxer_cleanup (hurd_cap_rpc_context_t ctx)
{
  hurd_cap_obj_drop (ctx->obj);
}


/* A worker thread for RPC processing.  The behaviour of this function
   is tightly integrated with the behaviour of the manager thread.  */
static void *
manage_mt_worker (void *arg, bool async)
{
  struct worker_info *info = (struct worker_info *) arg;
  hurd_cap_bucket_t bucket = info->bucket;
  struct _hurd_cap_list_item worker_item;
  _hurd_cap_list_item_t worker = &worker_item;
  l4_thread_id_t manager = info->manager_tid;
  l4_time_t timeout = info->timeout;
  l4_thread_id_t from;
  l4_msg_tag_t msg_tag;
  bool current_worker_is_us;

  /* Prepare the worker queue item.  [SYNC: As we are always the
     current worker thread when we are started up, we do not add
     ourselves to the free list.]  */
  worker->thread = pthread_self ();
  worker->tid = l4_myself ();
  worker->next = NULL;
  worker->prevp = NULL;

  if (EXPECT_FALSE (async))
    {
      /* We have to add ourselves to the free list and inform the
	 worker_alloc_async thread.  */
      pthread_mutex_lock (&bucket->lock);

      if (bucket->is_manager_waiting && !bucket->free_worker)
	{
	  /* The manager is starving for worker threads.  */
	  pthread_cond_broadcast (&bucket->cond);
	}
      _hurd_cap_list_item_add (&bucket->free_worker, worker);

      /* Notify the worker_alloc_async thread that we have started up
	 and added ourselves to the free list.  */
      bucket->worker_alloc_state = _HURD_CAP_STATE_RED;

      /* This will wake up the worker_alloc_async thread, but also the
	 manager in case it is blocked on getting a new worker
	 thread.  */
      pthread_cond_broadcast (&bucket->cond);
      pthread_mutex_unlock (&bucket->lock);

      /* We do not know if we will be the current worker thread or
	 not, so we must wait with a timeout.  */
      msg_tag = l4_xreceive_timeout (manager, timeout, &from);
    }
  else
    {
      /* When we are started up, we are supposed to listen as soon as
	 possible to the next incoming message.  When we know we are the
	 current worker thread, we do this without a timeout.  */
      msg_tag = l4_xreceive (manager, &from);
    }

  while (1)
    {
      if (EXPECT_FALSE (l4_ipc_failed (msg_tag)))
	{
	  /* Slow path.  */

	  l4_word_t err_code = l4_error_code ();
	  l4_word_t ipc_err = (err_code >> 1) & 0x7;

	  if (ipc_err == L4_IPC_CANCELED || ipc_err == L4_IPC_ABORTED)
	    /* We have been canceled for shutdown.  */
	    break;

	  /* The only other error that can happen is a timeout waiting
	     for the message.  */
	  assert (ipc_err == L4_IPC_TIMEOUT);

	  pthread_mutex_lock (&bucket->lock);
	  /* If we are not on the free queue, we are the current worker.  */
	  current_worker_is_us = _hurd_cap_list_item_dequeued (worker);
	  pthread_mutex_unlock (&bucket->lock);

	  /* If we are not the current worker, then we can just exit
	     now because of our timeout.  */
	  if (!current_worker_is_us)
	    break;

	  /* If we are the current worker, we should wait here for the
	     next message without a timeout.  */
	  msg_tag = l4_xreceive (manager, &from);
	  /* From here, we will loop all over to the beginning of the
	     while(1) block.  */
	}
      else
	{
	  /* Fast path.  Process the RPC.  */
	  error_t err = 0;
	  struct hurd_cap_rpc_context ctx;
	  bool inhibited = false;

	  /* IMPORTANT NOTE: The manager thread is blocked until we
	     reply a message with a label MSG_ACCEPTED or
	     MSG_REJECTED.  We are supposed to return such a message
	     as quickly as possible.  In the accepted case, we should
	     then process the message, while in the rejected case we
	     should rapidly go into the next receive.  */

	  /* Before we can work on the message, we need to copy it.
	     This is because the MRs holding the message might be
	     overridden by the pthread implementation or other
	     function calls we make.  In particular,
	     pthread_mutex_lock is can mangle the message buffer.  */
	  l4_msg_store (msg_tag, ctx.msg);

	  assert (l4_ipc_propagated (msg_tag));
	  assert (l4_is_thread_equal (l4_actual_sender (), manager));

	  pthread_mutex_lock (&bucket->lock);
	  /* We process cancellation messages regardless of the
	     bucket state.  */
	  if (EXPECT_FALSE (l4_msg_label (ctx.msg)
			    == HURD_CAP_MSG_LABEL_CANCEL))
	    {
	      if (l4_untyped_words (l4_msg_msg_tag (ctx.msg)) == 1)
		{
		  l4_thread_id_t tid = l4_msg_word (ctx.msg, 0);
		  
		  /* First verify access.  Threads are only allowed to
		     cancel RPCs from other threads in the task.  */
		  if (hurd_task_id_from_thread_id (tid)
		      == hurd_task_id_from_thread_id (from))
		    {
		      /* We allow cancel requests even if normal RPCs
			 are inhibited.  */
		      _hurd_cap_list_item_t pending_worker;
		      
		      pending_worker = hurd_ihash_find (&bucket->senders,
							tid);
		      if (!pending_worker)
			reply_err (from, ESRCH);
		      else
			{
			  /* Found it.  Cancel it.  */
			  pthread_cancel (pending_worker->thread);
			  /* Reply success.  */
			  reply_err (from, 0);
			}
		    }
		}
	      /* Set the error variable so that we return to the
		 manager immediately.  */
	      err = ECAP_NOREPLY;
	    }
	  else
	    {
	      /* Normal RPCs.  */
	      if (EXPECT_FALSE (bucket->state == _HURD_CAP_STATE_BLACK))
		{
		  /* The bucket operations have been ended, and the
		     manager has already been canceled.  We know that
		     the BUCKET->senders hash is empty, so we can
		     quickly process the message.  */

		  /* This is a normal RPC.  We cancel it immediately.  */
		  reply_err (from, ECANCELED);

		  /* Now set ERR to any error, so we return to the
		     manager.  */
		  err = ECAP_NOREPLY;	/* Doesn't matter which error.  */
		}
	      else
		{
		  if (EXPECT_FALSE (bucket->state != _HURD_CAP_STATE_GREEN))
		    {
		      /* If we are inhibited, we will have to wait
			 until we are uninhibited.  */
		      inhibited = true;
		    }

		  /* FIXME: This is inefficient.  ihash should support
		     an "add if not there" function.  */
		  if (EXPECT_FALSE (hurd_ihash_find (&bucket->senders, from)))
		    err = EBUSY;
		  else
		    {
		      /* FIXME: We know intimately that pthread_self is not
			 _HURD_IHASH_EMPTY or _HURD_IHASH_DELETED.  */
		      err = hurd_ihash_add (&bucket->senders, from, worker);
		    }
		}
	    }
	    
	  if (EXPECT_FALSE (err))
	    {
	      pthread_mutex_unlock (&bucket->lock);

	      /* Either we already processed the message above, or
		 this user thread is currently in an RPC.  We don't
		 allow asynchronous operation for security reason
		 (preventing DoS attacks).  Silently drop the
		 message.  */
	      msg_tag = l4_niltag;
	      l4_set_msg_label (&msg_tag, _HURD_CAP_MSG_WORKER_REJECTED);
	      l4_load_mr (0, msg_tag);

	      /* Reply to the manager that we don't accept the message
		 and wait for the next message without a timeout
		 (because now we know we are the current worker).  */
	      from = manager;
	      msg_tag = l4_lreply_wait (manager, &from);

	      /* From here, we will loop all over to the beginning of
		 the while(1) block.  */
	    }
	  else
	    {
	      _hurd_cap_list_item_add (inhibited ? &bucket->waiting_rpcs
				       : &bucket->pending_rpcs, worker);
	      pthread_mutex_unlock (&bucket->lock);

	      msg_tag = l4_niltag;
	      l4_set_msg_label (&msg_tag, _HURD_CAP_MSG_WORKER_ACCEPTED);
	      l4_load_mr (0, msg_tag);
	      msg_tag = l4_reply (manager);
	      assert (l4_ipc_succeeded (msg_tag));

	      /* Now we are "detached" from the manager in the sense
		 that we are not the current worker thread
		 anymore.  */

	      if (EXPECT_FALSE (inhibited))
		{
		  pthread_mutex_lock (&bucket->lock);
		  while (!err && bucket->state != _HURD_CAP_STATE_GREEN
			 && bucket->state != _HURD_CAP_STATE_BLACK)
		    err = hurd_cond_wait (&bucket->cond, &bucket->lock);
		  if (!err)
		    {
		      if (bucket->state == _HURD_CAP_STATE_BLACK)
			err = ECANCELED;
		      else 
			{
			  /* State is _HURD_CAP_STATE_GREEN.  Move
			     ourselves to the pending RPC list.  */
			  _hurd_cap_list_item_remove (worker);
			  _hurd_cap_list_item_add (&bucket->pending_rpcs,
						   worker);
			}
		    }
		  pthread_mutex_unlock (&bucket->lock);
		}

	      if (EXPECT_TRUE (!err))
		{
		  /* Process the message.  */
		  ctx.sender = hurd_task_id_from_thread_id (from);
		  ctx.bucket = bucket;
		  ctx.from = from;
		  ctx.obj = NULL;
		  err = manage_demuxer (&ctx, worker);
		}

	      /* Post-processing.  */

	      pthread_mutex_lock (&bucket->lock);
	      /* We have to add ourselves to the free list before (or
		 at the same time) as removing the client from the
		 pending hash, and before replying to the RPC (if we
		 reply in the worker thread at all).  The former is
		 necessary to make sure that no new thread is created
		 in the race that would otherwise exist, namely after
		 replying and before adding ourself to the free list.
		 The latter is required because a client that
		 immediately follows up with a new message of course
		 can expect that to work properly.  */

	      if (EXPECT_FALSE (bucket->is_manager_waiting
				&& !bucket->free_worker))
		{
		  /* The manager is starving for worker threads.  */
		  pthread_cond_broadcast (&bucket->cond);
		}

	      /* Remove from pending_rpcs (or waiting_rpcs) list.  */
	      _hurd_cap_list_item_remove (worker);
	      /* The last waiting RPC may have to signal the manager.  */
	      if (EXPECT_FALSE (inhibited
				&& bucket->state == _HURD_CAP_STATE_BLACK
				&& !bucket->waiting_rpcs))
		pthread_cond_broadcast (&bucket->cond);
	      _hurd_cap_list_item_add (&bucket->free_worker, worker);

	      _hurd_cap_bucket_cond_check (bucket);

	      /* Now that we are back on the free list it is safe to
		 let in the next RPC by this thread.  */
	      hurd_ihash_locp_remove (&bucket->senders, worker->locp);

	      /* FIXME: Reap the cancellation flag here.  If it was
		 set, we have been effectively unblocked now.  From
		 now on, canceling us means something different than
		 cancelling a pending RPC (it means terminating the
		 worker thread).  */

	      pthread_mutex_unlock (&bucket->lock);

	      /* Finally, return the reply message, if appropriate.  */
	      if (EXPECT_TRUE (err != ECAP_NOREPLY))
		{
		  if (EXPECT_FALSE (err))
		    reply_err (from, err);
		  else
		    {
		      /* We must make sure the message tag is set.  */
		      l4_msg_tag_t tag = l4_msg_msg_tag (ctx.msg);
		      l4_clear_propagation (&tag);
		      l4_set_msg_msg_tag (ctx.msg, tag);
		      l4_msg_load (ctx.msg);
		      l4_reply (from);
		    }
		}

	      if (ctx.obj)
		manage_demuxer_cleanup (&ctx);
	      
	      /* Now listen for the next message, with a timeout.  */
	      from = manager;
	      msg_tag = l4_xreceive_timeout (manager, timeout, &from);

	      /* From here, we will loop to the beginning of the
		 while(1) block.  */
	    }
	}
    }

  /* At this point, we have been canceled while being on the free
     list, so we should go away.  */

  pthread_mutex_lock (&bucket->lock);
  if (_hurd_cap_list_item_dequeued (worker))
    {
      /* We are the current worker thread.  We are the last worker
	 thread the manager thread will cancel.  */
      pthread_cond_broadcast (&bucket->cond);
    }
  else
    {
      _hurd_cap_list_item_remove (worker);
      if (bucket->is_manager_waiting && !bucket->free_worker)
	{
	  /* The manager is shutting down.  We are the last free
	     worker (except for the current worker thread) to be
	     canceled.  */
	  pthread_cond_broadcast (&bucket->cond);
	}
    }
  pthread_mutex_unlock (&bucket->lock);

  return NULL;
}


/* A worker thread for RPC processing.  The behaviour of this function
   is tightly integrated with the behaviour of the manager thread.  */
static void *
manage_mt_worker_sync (void *arg)
{
  return manage_mt_worker (arg, false);
}


/* A worker thread for RPC processing.  The behaviour of this function
   is tightly integrated with the behaviour of the manager thread.  */
static void *
manage_mt_worker_async (void *arg)
{
  return manage_mt_worker (arg, true);
}


/* Return the next free worker thread.  If no free worker thread is
   available, create a new one.  If that fails, block until one
   becomes free.  If we are interrupted while blocking, return
   l4_nilthread.  */
static l4_thread_id_t
manage_mt_get_next_worker (struct worker_info *info, pthread_t *worker_thread)
{
  hurd_cap_bucket_t bucket = info->bucket;
  l4_thread_id_t worker = l4_nilthread;
  _hurd_cap_list_item_t worker_item;

  pthread_mutex_lock (&bucket->lock);

  if (EXPECT_FALSE (bucket->free_worker == NULL))
    {
      /* Slow path.  Create a new thread and use that.  */
      error_t err;

      pthread_mutex_unlock (&bucket->lock);
      worker = pthread_pool_get_np ();
      if (worker == l4_nilthread)
	err = EAGAIN;
      else
	{
	  err = pthread_create_from_l4_tid_np (worker_thread, NULL,
					       worker, manage_mt_worker_sync,
					       info);
	  /* Return the thread to the pool.  */
	  if (err)
	    pthread_pool_add_np (worker);
	}

      if (!err)
	{
	  pthread_detach (*worker_thread);
	  return worker;
	}
      else
	{
	  pthread_mutex_lock (&bucket->lock);
	  if (!bucket->free_worker)
	    {
	      /* Creating a new thread failed.  As a last resort, put
		 ourself to sleep until we are woken up by the next
		 free worker.  Hopefully not all workers are blocking
		 forever.  */

	      /* FIXME: To fix the case where all workers are blocking
		 forever, cancel one (or more? all?) (random? oldest?)
		 worker threads.  Usually, that user will restart, but
		 it will nevertheless allow us to make some (although
		 slow) process.  */

	      /* The next worker thread that adds itself to the free
		 list will broadcast the condition.  */
	      bucket->is_manager_waiting = true;
	      do
		err = hurd_cond_wait (&bucket->cond, &bucket->lock);
	      while (!err && !bucket->free_worker);

	      if (err)
		{
		  pthread_mutex_unlock (&bucket->lock);
		  return l4_nilthread;
		}
	    }
	}
    }

  /* Fast path.  A worker thread is available.  Remove it from the
     free list and use it.  */
  worker_item = bucket->free_worker;
  _hurd_cap_list_item_remove (worker_item);
  pthread_mutex_unlock (&bucket->lock);

  *worker_thread = worker_item->thread;
  return worker_item->tid;
}


/* A worker thread for allocating new worker threads.  Only used if
   asynchronous worker thread allocation is requested.  This is only
   necessary (and useful) for physmem, to break out of a potential
   dead-lock with the task server.  */
static void *
worker_alloc_async (void *arg)
{
  struct worker_info *info = (struct worker_info *) arg;
  hurd_cap_bucket_t bucket = info->bucket;
  error_t err;

  pthread_mutex_lock (&bucket->lock);
  if (bucket->state == _HURD_CAP_STATE_BLACK)
    {
      pthread_mutex_unlock (&bucket->lock);
      return NULL;
    }

  while (1)
    {
      err = hurd_cond_wait (&bucket->cond, &bucket->lock);
      /* We ignore the error, as the only error that can occur is
	 ECANCELED, and only if the bucket state has gone to black for
	 shutdown.  */
      if (bucket->state == _HURD_CAP_STATE_BLACK)
	break;

      if (bucket->worker_alloc_state == _HURD_CAP_STATE_GREEN)
	{
	  l4_thread_id_t worker = l4_nilthread;
	  pthread_t worker_thread;

	  pthread_mutex_unlock (&bucket->lock);

	  worker = pthread_pool_get_np ();
	  if (worker == l4_nilthread)
	    err = EAGAIN;
	  else
	    {
	      err = pthread_create_from_l4_tid_np (&worker_thread, NULL,
						   worker,
						   manage_mt_worker_async,
						   info);
	      /* Return the thread to the pool.  */
	      if (err)
		pthread_pool_add_np (worker);
	    }

	  if (!err)
	    {
	      pthread_detach (worker_thread);

	      pthread_mutex_lock (&bucket->lock);
	      bucket->worker_alloc_state = _HURD_CAP_STATE_YELLOW;
	      /* We ignore any error, as the only error that can occur
		 is ECANCELED, and only if the bucket state goes to
		 black for shutdown.  But particularly in that case we
		 want to wait until the thread has fully come up and
		 entered the free list, so it's properly accounted for
		 and will be canceled at shutdown by the manager.  */
	      while (bucket->worker_alloc_state == _HURD_CAP_STATE_YELLOW)
		err = hurd_cond_wait (&bucket->cond, &bucket->lock);

	      /* Will be set by the started thread.  */
	      assert (bucket->worker_alloc_state == _HURD_CAP_STATE_RED);
	    }
	  else
	    {
	      pthread_mutex_lock (&bucket->lock);
	      bucket->worker_alloc_state = _HURD_CAP_STATE_RED;
	    }

	  if (bucket->state == _HURD_CAP_STATE_BLACK)
	    break;
	}
    }

  bucket->worker_alloc_state = _HURD_CAP_STATE_BLACK;
  pthread_mutex_unlock (&bucket->lock);

  return NULL;
}
  


/* Start managing RPCs on the bucket BUCKET.  The ROOT capability
   object, which must be unlocked and have one reference throughout
   the whole time this function runs, is used for bootstrapping client
   connections.  The GLOBAL_TIMEOUT parameter specifies the number of
   seconds until the manager times out (if there are no active users).
   The WORKER_TIMEOUT parameter specifies the number of seconds until
   each worker thread times out (if there are no RPCs processed by the
   worker thread).

   If this returns ECANCELED, then hurd_cap_bucket_end was called with
   the force flag being true while there were still active users.  If
   this returns without any error, then the timeout expired, or
   hurd_cap_bucket_end was called without active users.  */
error_t
hurd_cap_bucket_manage_mt (hurd_cap_bucket_t bucket,
			   hurd_cap_obj_t root,
			   unsigned int global_timeout_sec,
			   unsigned int worker_timeout_sec)
{
  error_t err;
  l4_time_t global_timeout;
  pthread_t worker_thread;
  l4_thread_id_t worker;
  struct worker_info info;
  _hurd_cap_list_item_t item;

  global_timeout = (global_timeout_sec == 0) ? L4_NEVER
    : l4_time_period (UINT64_C (1000000) * global_timeout_sec);

  info.bucket = bucket;
  info.manager_tid = l4_myself ();
  info.timeout = (worker_timeout_sec == 0) ? L4_NEVER
    : l4_time_period (UINT64_C (1000000) * worker_timeout_sec);

  /* We create the first worker thread ourselves, to catch any
     possible error at this stage and bail out properly if needed.  */
  worker = pthread_pool_get_np ();
  if (worker == l4_nilthread)
    return EAGAIN;
  err = pthread_create_from_l4_tid_np (&worker_thread, NULL,
				       worker, manage_mt_worker_sync, &info);
  if (err)
    {
      /* Return the thread to the pool.  */
      pthread_pool_add_np (worker);
      return err;
    }
  pthread_detach (worker_thread);

  pthread_mutex_lock (&bucket->lock);
  if (bucket->is_worker_alloc_async)
    {
      /* Prevent creation of new worker threads initially.  */
      bucket->worker_alloc_state = _HURD_CAP_STATE_RED;

      /* Asynchronous worker thread allocation is requested.  */
      err = pthread_create (&bucket->worker_alloc, NULL,
			    worker_alloc_async, &info);
      
      if (err)
	{
	  /* Cancel the worker thread.  */
	  pthread_cancel (worker_thread);
	  hurd_cond_wait (&bucket->cond, &bucket->lock);
	  pthread_mutex_unlock (&bucket->lock);
	  return err;
	}
    }
  bucket->manager = pthread_self ();
  bucket->is_managed = true;
  bucket->is_manager_waiting = false;
  pthread_mutex_unlock (&bucket->lock);
  
  while (1)
    {
      l4_thread_id_t from = l4_anythread;
      l4_msg_tag_t msg_tag;

      /* We never accept any map or grant items.  FIXME: For now, we
	 also do not accept any string buffer items.  */
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

      /* Because we do not accept any string items, we do not actually
	 need to set the Xfer timeouts.  But this is what we want to set
	 them to when we eventually do support string items.  */
      l4_set_xfer_timeouts (l4_timeouts (L4_ZERO_TIME, L4_ZERO_TIME));

      /* FIXME: Make sure we have enabled deferred cancellation, and
	 use an L4 ipc() stub that supports that.  In fact, this must
	 be true for most of the IPC operations in this file.  */
      msg_tag = l4_wait_timeout (global_timeout, &from);

      if (EXPECT_FALSE (l4_ipc_failed (msg_tag)))
	{
	  l4_word_t err_code = l4_error_code ();

	  /* FIXME: We need a macro or inline function for that.  */
	  l4_word_t ipc_err = (err_code >> 1) & 0x7;

	  /* There are two possible errors, cancellation or timeout.
	     Any other error indicates a bug in the code.  */
	  if (ipc_err == L4_IPC_CANCELED || ipc_err == L4_IPC_ABORTED)
	    {
	      /* If we are canceled, then this means that our state is
		 now _HURD_CAP_STATE_BLACK and we should end managing
		 RPCs even if there are still active users.  */

	      pthread_mutex_lock (&bucket->lock);
	      assert (bucket->state == _HURD_CAP_STATE_BLACK);
	      err = ECANCELED;
	      break;
	    }
	  else
	    {
	      assert (((err_code >> 1) & 0x7) == L4_IPC_TIMEOUT);

	      pthread_mutex_lock (&bucket->lock);
	      /* Check if we can time out safely.  */
	      if (bucket->state == _HURD_CAP_STATE_GREEN
		  && !bucket->nr_caps && !bucket->pending_rpcs
		  && !bucket->waiting_rpcs)
		{
		  err = 0;
		  break;
		}
	      pthread_mutex_unlock (&bucket->lock);
	    }
	}
      else
	{
	  /* Propagate the message to the worker thread.  */
	  l4_set_propagation (&msg_tag);
	  l4_set_virtual_sender (from);
	  l4_set_msg_tag (msg_tag);

	  /* FIXME: Make sure to use a non-cancellable l4_lcall that
	     does preserve any pending cancellation flag for this
	     thread.  Alternatively, we can handle cancellation here
	     (reply ECANCELED to user, and enter shutdown
	     sequence.  */
	  msg_tag = l4_lcall (worker);
	  assert (l4_ipc_succeeded (msg_tag));

	  if (EXPECT_TRUE (l4_label (msg_tag)
			   == _HURD_CAP_MSG_WORKER_ACCEPTED))
	    {
	      worker = manage_mt_get_next_worker (&info, &worker_thread);
	      if (worker == l4_nilthread)
		{
		  /* The manage_mt_get_next_worker thread was
		     canceled.  In this case we have to terminate
		     ourselves.  */
		  err = hurd_cap_bucket_inhibit (bucket);
		  assert (!err);
		  hurd_cap_bucket_end (bucket, true);

		  pthread_mutex_lock (&bucket->lock);
		  err = ECANCELED;
		  break;
		}
	    }
	}
    }
        
  /* At this point, bucket->lock is held.  Start the shutdown
     sequence.  */
  assert (!bucket->pending_rpcs);

  /* First shutdown the allocator thread, if any.  */
  if (bucket->is_worker_alloc_async)
    {
      pthread_cancel (bucket->worker_alloc);
      pthread_join (bucket->worker_alloc, NULL);
    }
      
  /* Now force all the waiting rpcs onto the free list.  They will
     have noticed the state change to _HURD_CAP_STATE_BLACK already,
     we just have to block until the last one wakes us up.  */
  while (bucket->waiting_rpcs)
    hurd_cond_wait (&bucket->cond, &bucket->lock);

  /* Cancel the free workers.  */
  item = bucket->free_worker;
  while (item)
    {
      pthread_cancel (item->thread);
      item = item->next;
    }

  /* Request the condition to be broadcasted.  */
  bucket->is_manager_waiting = true;

  while (bucket->free_worker)
    {
      /* We ignore cancellations at this point, because we are already
	 shutting down.  */
      hurd_cond_wait (&bucket->cond, &bucket->lock);
    }

  /* Now cancel the current worker, except if we were canceled while
     trying to get a new one (in which case there is no current
     worker).  */
  if (worker != l4_nilthread)
    {
      pthread_cancel (worker_thread);
      hurd_cond_wait (&bucket->cond, &bucket->lock);
    }

  bucket->is_managed = false;
  pthread_mutex_unlock (&bucket->lock);

  return err;
}
