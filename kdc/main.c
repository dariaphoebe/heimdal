/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kdc_locl.h"
#ifdef HAVE_UTIL_H
#include <util.h>
#endif

sig_atomic_t exit_flag = 0;

#ifdef SUPPORT_DETACH
int detach_from_console = -1;
#endif

static RETSIGTYPE
sigterm(int sig)
{
    exit_flag = sig;
}

/*
 * Allow dropping root bit, since heimdal reopens the database all the
 * time the database needs to be owned by the user you are switched
 * too. A better solution is to split the kdc in to more processes and
 * run the network facing part with very low privilege.
 */

static void
switch_environment(void)
{
    if ((runas_string || chroot_string) && geteuid() != 0)
	errx(1, "no running as root, can't switch user/chroot");

    if (chroot_string && chroot(chroot_string) != 0)
	errx(1, "chroot(%s)", "chroot_string failed");

    if (runas_string) {
	struct passwd *pw;

	pw = getpwnam(runas_string);
	if (pw == NULL)
	    errx(1, "unknown user %s", runas_string);
	
	if (initgroups(pw->pw_name, pw->pw_gid) < 0)
	    err(1, "initgroups failed");
	
	if (setgid(pw->pw_gid) < 0)
	    err(1, "setgid(%s) failed", runas_string);
	
	if (setuid(pw->pw_uid) < 0)
	    err(1, "setuid(%s)", runas_string);
    }
}


int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_kdc_configuration *config;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret == KRB5_CONFIG_BADFORMAT)
	errx (1, "krb5_init_context failed to parse configuration file");
    else if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    ret = krb5_kt_register(context, &hdb_kt_ops);
    if (ret)
	errx (1, "krb5_kt_register(HDB) failed: %d", ret);

    config = configure(context, argc, argv);

#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sigterm;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGXCPU, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
    }
#else
    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);
    signal(SIGXCPU, sigterm);
    signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SUPPORT_DETACH
    if (detach_from_console)
	daemon(0, 0);
#endif
    pidfile(NULL);

    switch_environment();

    loop(context, config);
    krb5_free_context(context);
    return 0;
}
