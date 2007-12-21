#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

const char program_name[] = "t-rpc";
int output_debug = 1;

#define RPC_STUB_PREFIX rpc
#define RPC_ID_PREFIX RPC
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET 1

#include <hurd/rpc.h>

/* Exception message ids.  */
enum
  {
    RPC_noargs = 500,
    RPC_onein,
    RPC_oneout,
    RPC_onlyin,
    RPC_onlyout,
    RPC_mix,
  };

struct foo
{
  int a;
  char b;
};

RPC(noargs, 0, 0)
RPC(onein, 1, 0, uint32_t, arg)
RPC(oneout, 0, 1, uint32_t, arg)
RPC(onlyin, 4, 0, uint32_t, arg, uint32_t, idx, struct foo, foo, bool, p)
RPC(onlyout, 0, 4, uint32_t, arg, uint32_t, idx, struct foo, foo, bool, p)
RPC(mix, 2, 3, uint32_t, arg, uint32_t, idx,
    struct foo, foo, bool, p, int, i)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

int
main (int argc, char *argv[])
{
  printf ("Checking RPC... ");

  l4_msg_t msg;
  error_t err;

  rpc_noargs_send_marshal (&msg);
  err = rpc_noargs_send_unmarshal (&msg);
  assert (! err);

  rpc_noargs_reply_marshal (&msg);
  err = rpc_noargs_reply_unmarshal (&msg);
  assert (err == 0);


#define VALUE 0xfde8963a
  uint32_t arg = VALUE;
  uint32_t arg_out;

  rpc_onein_send_marshal (&msg, arg);
  err = rpc_onein_send_unmarshal (&msg, &arg_out);
  assert (! err);
  assert (arg_out == VALUE);

  rpc_onein_reply_marshal (&msg);
  err = rpc_onein_reply_unmarshal (&msg);
  assert (! err);


  rpc_oneout_send_marshal (&msg);
  err = rpc_oneout_send_unmarshal (&msg);
  assert (! err);

  rpc_oneout_reply_marshal (&msg, arg);
  err = rpc_oneout_reply_unmarshal (&msg, &arg_out);
  assert (! err);
  assert (arg_out == VALUE);


  struct foo foo;
  foo.a = 1 << 31;
  foo.b = 'l';
  uint32_t idx_out;
  struct foo foo_out;
  bool p_out;

  rpc_onlyin_send_marshal (&msg, 0x1234567, 321, foo, true);
  err = rpc_onlyin_send_unmarshal (&msg, &arg_out, &idx_out, &foo_out, &p_out);
  assert (! err);
  assert (arg_out == 0x1234567);
  assert (idx_out == 321);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == true);

  rpc_onlyin_reply_marshal (&msg);
  err = rpc_onlyin_reply_unmarshal (&msg);
  assert (! err);


  rpc_onlyout_send_marshal (&msg);
  err = rpc_onlyout_send_unmarshal (&msg);
  assert (! err);

  rpc_onlyout_reply_marshal (&msg, 0x1234567, 321, foo, true);
  err = rpc_onlyout_reply_unmarshal (&msg, &arg_out, &idx_out,
				     &foo_out, &p_out);
  assert (! err);
  assert (arg_out == 0x1234567);
  assert (idx_out == 321);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == true);


  rpc_mix_send_marshal (&msg, arg, 456789);
  err = rpc_mix_send_unmarshal (&msg, &arg_out, &idx_out);
  assert (! err);
  assert (arg_out == arg);
  assert (idx_out == 456789);

  int i_out = 0;
  rpc_mix_reply_marshal (&msg, foo, false, 4200042);
  err = rpc_mix_reply_unmarshal (&msg, &foo_out, &p_out, &i_out);
  assert (! err);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == false);
  assert (i_out == 4200042);

  printf ("ok\n");
  return 0;
}
