#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

char *program_name = "t-rpc";
int output_debug = 1;

#define RPC_STUB_PREFIX rpc
#define RPC_ID_PREFIX RPC

#include <viengoos/rpc.h>

/* Exception message ids.  */
enum
  {
    RPC_noargs = 0x1ABE100,
    RPC_onein,
    RPC_oneout,
    RPC_onlyin,
    RPC_onlyout,
    RPC_mix,
    RPC_caps,
  };

struct foo
{
  int a;
  char b;
};

RPC(noargs, 0, 0, 0)
RPC(onein, 1, 0, 0, uint32_t, arg)
RPC(oneout, 0, 1, 0, uint32_t, arg)
RPC(onlyin, 4, 0, 0, uint32_t, arg, uint32_t, idx, struct foo, foo, bool, p)
RPC(onlyout, 0, 4, 0, uint32_t, arg, uint32_t, idx, struct foo, foo, bool, p)
RPC(mix, 2, 3, 0, uint32_t, arg, uint32_t, idx,
    struct foo, foo, bool, p, int, i)
RPC(caps, 3, 2, 2,
    /* In: */
    int, i, cap_t, c, struct foo, foo,
    /* Out: */
    int, a, int, b, cap_t, x, cap_t, y)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

int
main (int argc, char *argv[])
{
  printf ("Checking RPC... ");

  error_t err;
  struct vg_message *msg;


#define REPLY VG_ADDR (0x1000, VG_ADDR_BITS - 12)
  vg_addr_t reply = REPLY;

  msg = malloc (sizeof (*msg));
  rpc_noargs_send_marshal (msg, REPLY);
  err = rpc_noargs_send_unmarshal (msg, &reply);
  assert (! err);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_noargs_reply_marshal (msg);
  err = rpc_noargs_reply_unmarshal (msg);
  assert (err == 0);
  free (msg);


  msg = malloc (sizeof (*msg));
#define VALUE 0xfde8963a
  uint32_t arg = VALUE;
  uint32_t arg_out;

  rpc_onein_send_marshal (msg, arg, REPLY);
  err = rpc_onein_send_unmarshal (msg, &arg_out, &reply);
  assert (! err);
  assert (arg_out == VALUE);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_onein_reply_marshal (msg);
  err = rpc_onein_reply_unmarshal (msg);
  assert (! err);
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_oneout_send_marshal (msg, REPLY);
  err = rpc_oneout_send_unmarshal (msg, &reply);
  assert (! err);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_oneout_reply_marshal (msg, arg);
  err = rpc_oneout_reply_unmarshal (msg, &arg_out);
  assert (! err);
  assert (arg_out == VALUE);
  free (msg);

  msg = malloc (sizeof (*msg));

  struct foo foo;
  foo.a = 1 << 31;
  foo.b = 'l';
  uint32_t idx_out;
  struct foo foo_out;
  bool p_out;

  rpc_onlyin_send_marshal (msg, 0x1234567, 0xABC, foo, true, REPLY);
  err = rpc_onlyin_send_unmarshal (msg, &arg_out, &idx_out, &foo_out, &p_out,
				   &reply);
  assert (! err);
  assert (arg_out == 0x1234567);
  assert (idx_out == 0xABC);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == true);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_onlyin_reply_marshal (msg);
  err = rpc_onlyin_reply_unmarshal (msg);
  assert (! err);
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_onlyout_send_marshal (msg, REPLY);
  err = rpc_onlyout_send_unmarshal (msg, &reply);
  assert (! err);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_onlyout_reply_marshal (msg, 0x1234567, 321, foo, true);
  err = rpc_onlyout_reply_unmarshal (msg, &arg_out, &idx_out,
				     &foo_out, &p_out);
  assert (! err);
  assert (arg_out == 0x1234567);
  assert (idx_out == 321);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == true);
  free (msg);


  msg = malloc (sizeof (*msg));
  rpc_mix_send_marshal (msg, arg, 456789, REPLY);
  err = rpc_mix_send_unmarshal (msg, &arg_out, &idx_out, &reply);
  assert (! err);
  assert (arg_out == arg);
  assert (idx_out == 456789);
  assert (VG_ADDR_EQ (reply, REPLY));
  free (msg);

  msg = malloc (sizeof (*msg));
  int i_out = 0;
  rpc_mix_reply_marshal (msg, foo, false, 4200042);
  err = rpc_mix_reply_unmarshal (msg, &foo_out, &p_out, &i_out);
  assert (! err);
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  assert (p_out == false);
  assert (i_out == 4200042);
  free (msg);

  msg = malloc (sizeof (*msg));
  rpc_caps_send_marshal (msg, 54, VG_ADDR (1, VG_ADDR_BITS), foo, REPLY);
  vg_addr_t addr;
  err = rpc_caps_send_unmarshal (msg, &i_out, &addr, &foo_out, &reply);
  assert (! err);
  assert (i_out == 54);
  assert (VG_ADDR_EQ (addr, VG_ADDR (1, VG_ADDR_BITS)));
  assert (foo_out.a == foo.a);
  assert (foo_out.b == foo.b);
  free (msg);

  printf ("ok\n");
  return 0;
}
