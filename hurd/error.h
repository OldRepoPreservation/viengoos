/* error.h - List of error codes.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef HURD_ERROR_H
#define HURD_ERROR_H

enum __error_t_codes
{
	EPERM = 1,
#define EPERM EPERM	 /* Operation not permitted */
	ENOENT = 2,
#define ENOENT ENOENT	 /* No such file or directory */
	ESRCH = 3,
#define ESRCH ESRCH	 /* No such process */
	EINTR = 4,
#define EINTR EINTR	 /* Interrupted system call */
	EIO = 5,
#define EIO EIO		 /* Input/output error */
	ENXIO = 6,
#define ENXIO ENXIO	 /* No such device or address */
	E2BIG = 7,
#define E2BIG E2BIG	 /* Argument list too long */
	ENOEXEC = 8,
#define ENOEXEC ENOEXEC	 /* Exec format error */
	EBADF = 9,
#define EBADF EBADF	 /* Bad file descriptor */
	ECHILD = 10,
#define ECHILD ECHILD	/* No child processes */
	EDEADLK = 11,
#define EDEADLK EDEADLK	/* Resource deadlock avoided */
	ENOMEM = 12,
#define ENOMEM ENOMEM	/* Cannot allocate memory */
	EACCES = 13,
#define EACCES EACCES	/* Permission denied */
	EFAULT = 14,
#define EFAULT EFAULT	/* Bad address */
	ENOTBLK = 15,
#define ENOTBLK ENOTBLK	/* Block device required */
	EBUSY = 16,
#define EBUSY EBUSY	/* Device or resource busy */
	EEXIST = 17,
#define EEXIST EEXIST	/* File exists */
	EXDEV = 18,
#define EXDEV EXDEV	/* Invalid cross-device link */
	ENODEV = 19,
#define ENODEV ENODEV	/* No such device */
	ENOTDIR = 20,
#define ENOTDIR ENOTDIR	/* Not a directory */
	EISDIR = 21,
#define EISDIR EISDIR	/* Is a directory */
	EINVAL = 22,
#define EINVAL EINVAL	/* Invalid argument */
	EMFILE = 24,
#define EMFILE EMFILE	/* Too many open files */
	ENFILE = 23,
#define ENFILE ENFILE	/* Too many open files in system */
	ENOTTY = 25,
#define ENOTTY ENOTTY	/* Inappropriate ioctl for device */
	ETXTBSY = 26,
#define ETXTBSY ETXTBSY	/* Text file busy */
	EFBIG = 27,
#define EFBIG EFBIG	/* File too large */
	ENOSPC = 28,
#define ENOSPC ENOSPC	/* No space left on device */
	ESPIPE = 29,
#define ESPIPE ESPIPE	/* Illegal seek */
	EROFS = 30,
#define EROFS EROFS	/* Read-only file system */
	EMLINK = 31,
#define EMLINK EMLINK	/* Too many links */
	EPIPE = 32,
#define EPIPE EPIPE	/* Broken pipe */
	EDOM = 33,
#define EDOM EDOM	/* Numerical argument out of domain */
	ERANGE = 34,
#define ERANGE ERANGE	/* Numerical result out of range */
	EAGAIN = 35,
#define EAGAIN EAGAIN	/* Resource temporarily unavailable */
#define EWOULDBLOCK EAGAIN
	EALREADY = 37,
#define EALREADY EALREADY	/* Operation already in progress */
	ENOTSOCK = 38,
#define ENOTSOCK ENOTSOCK	/* Socket operation on non-socket */
	EMSGSIZE = 40,
#define EMSGSIZE EMSGSIZE	/* Message too long */
	EPROTOTYPE = 41,
#define EPROTOTYPE EPROTOTYPE	/* Protocol wrong type for socket */
	ENOPROTOOPT = 42,
#define ENOPROTOOPT ENOPROTOOPT	/* Protocol not available */
	EPROTONOSUPPORT = 43,
#define EPROTONOSUPPORT EPROTONOSUPPORT	/* Protocol not supported */
	ESOCKTNOSUPPORT = 44,
#define ESOCKTNOSUPPORT ESOCKTNOSUPPORT	/* Socket type not supported */
	EOPNOTSUPP = 45,
#define EOPNOTSUPP EOPNOTSUPP	/* Operation not supported */
	EPFNOSUPPORT = 46,
#define EPFNOSUPPORT EPFNOSUPPORT	/* Protocol family not supported */
	EAFNOSUPPORT = 47,
#define EAFNOSUPPORT EAFNOSUPPORT	/* Address family not supported by protocol */
	EADDRINUSE = 48,
#define EADDRINUSE EADDRINUSE	/* Address already in use */
	EADDRNOTAVAIL = 49,
#define EADDRNOTAVAIL EADDRNOTAVAIL	/* Cannot assign requested address */
	ENETDOWN = 50,
#define ENETDOWN ENETDOWN	/* Network is down */
	ENETUNREACH = 51,
#define ENETUNREACH ENETUNREACH	/* Network is unreachable */
	ENETRESET = 52,
#define ENETRESET ENETRESET	/* Network dropped connection on reset */
	ECONNABORTED = 53,
#define ECONNABORTED ECONNABORTED	/* Software caused connection abort */
	ECONNRESET = 54,
#define ECONNRESET ECONNRESET	/* Connection reset by peer */
	ENOBUFS = 55,
#define ENOBUFS ENOBUFS		/* No buffer space available */
	EISCONN = 56,
#define EISCONN EISCONN		/* Transport endpoint is already connected */
	ENOTCONN = 57,
#define ENOTCONN ENOTCONN	/* Transport endpoint is not connected */
	EDESTADDRREQ = 39,
#define EDESTADDRREQ EDESTADDRREQ	/* Destination address required */
	ESHUTDOWN = 58,
#define ESHUTDOWN ESHUTDOWN	/* Cannot send after transport endpoint shutdown */
	ETOOMANYREFS = 59,
#define ETOOMANYREFS ETOOMANYREFS	/* Too many references: cannot splice */
	ETIMEDOUT = 60,
#define ETIMEDOUT ETIMEDOUT	/* Connection timed out */
	ECONNREFUSED = 61,
#define ECONNREFUSED ECONNREFUSED	/* Connection refused */
	ELOOP = 62,
#define ELOOP ELOOP		/* Too many levels of symbolic links */
	ENAMETOOLONG = 63,
#define ENAMETOOLONG ENAMETOOLONG	/* File name too long */
	EHOSTDOWN = 64,
#define EHOSTDOWN EHOSTDOWN		/* Host is down */
	EHOSTUNREACH = 65,
#define EHOSTUNREACH EHOSTUNREACH	/* No route to host */
	ENOTEMPTY = 66,
#define ENOTEMPTY ENOTEMPTY	/* Directory not empty */
	EPROCLIM = 67,
#define EPROCLIM EPROCLIM	/* Too many processes */
	EUSERS = 68,
#define EUSERS EUSERS		/* Too many users */
	EDQUOT = 69,
#define EDQUOT EDQUOT		/* Disk quota exceeded */
	ESTALE = 70,
#define ESTALE ESTALE		/* Stale NFS file handle */
	EREMOTE = 71,
#define EREMOTE EREMOTE		/* Object is remote */
	EBADRPC = 72,
#define EBADRPC EBADRPC		/* RPC struct is bad */
	ERPCMISMATCH = 73,
#define ERPCMISMATCH ERPCMISMATCH	/* RPC version wrong */
	EPROGUNAVAIL = 74,
#define EPROGUNAVAIL EPROGUNAVAIL	/* RPC program not available */
	EPROGMISMATCH = 75,
#define EPROGMISMATCH EPROGMISMATCH	/* RPC program version wrong */
	EPROCUNAVAIL = 76,
#define EPROCUNAVAIL EPROCUNAVAIL	/* RPC bad procedure for program */
	ENOLCK = 77,
#define ENOLCK ENOLCK		/* No locks available */
	EFTYPE = 79,
#define EFTYPE EFTYPE		/* Inappropriate file type or format */
	EAUTH = 80,
#define EAUTH EAUTH		/* Authentication error */
	ENEEDAUTH = 81,
#define ENEEDAUTH ENEEDAUTH	/* Need authenticator */
	ENOSYS = 78,
#define ENOSYS ENOSYS		/* Function not implemented */
	ENOTSUP = 118,
#define ENOTSUP ENOTSUP		/* Not supported */
	EILSEQ = 106,
#define EILSEQ EILSEQ		/* Invalid or incomplete multibyte or wide character */
	EBACKGROUND = 100,
#define EBACKGROUND EBACKGROUND	/* Inappropriate operation for background process */
	EDIED = 101,
#define EDIED EDIED		/* Translator died */
	ED = 102,
#define ED ED			/* ? */
	EGREGIOUS = 103,
#define EGREGIOUS EGREGIOUS	/* You really blew it this time */
	EIEIO = 104,
#define EIEIO EIEIO		/* Computer bought the farm */
	EGRATUITOUS = 105,
#define EGRATUITOUS EGRATUITOUS	/* Gratuitous error */
	EBADMSG = 107,
#define EBADMSG EBADMSG		/* Bad message */
	EIDRM = 108,
#define EIDRM EIDRM		/* Identifier removed */
	EMULTIHOP = 109,
#define EMULTIHOP EMULTIHOP	/* Multihop attempted */
	ENODATA = 110,
#define ENODATA ENODATA		/* No data available */
	ENOLINK = 111,
#define ENOLINK ENOLINK		/* Link has been severed */
	ENOMSG = 112,
#define ENOMSG ENOMSG		/* No message of desired type */
	ENOSR = 113,
#define ENOSR ENOSR		/* Out of streams resources */
	ENOSTR = 114,
#define ENOSTR ENOSTR		/* Device not a stream */
	EOVERFLOW = 115,
#define EOVERFLOW EOVERFLOW	/* Value too large for defined data type */
	EPROTO = 116,
#define EPROTO EPROTO		/* Protocol error */
	ETIME = 117,
#define ETIME ETIME		/* Timer expired */
	ECANCELED = 118,
#define ECANCELED ECANCELED	/* Operation canceled */
};

#endif /* HURD_ERROR_H  */
