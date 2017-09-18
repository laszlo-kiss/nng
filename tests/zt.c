//
// Copyright 2017 Garrett D'Amore <garrett@damore.org>
// Copyright 2017 Capitar IT Group BV <info@capitar.com>
//
// This software is supplied under the terms of the MIT License, a
// copy of which should be located in the distribution where this
// file was obtained (LICENSE.txt).  A copy of the license may also be
// found online at https://opensource.org/licenses/MIT.
//

#include "convey.h"
#include "trantest.h"

extern int         nng_zt_register(void);
extern const char *nng_opt_zt_home;
extern int         nng_optid_zt_home;
extern int         nng_optid_zt_node;
extern int         nng_optid_zt_network_status;
extern int         nng_optid_zt_network_name;

// zerotier tests.

// This network is an open network setup exclusively for nng testing.
// Do not attach to it in production.
#define NWID "a09acf02337b057b"

#ifdef _WIN32

int
mkdir(const char *path, int mode)
{
	CreateDirectory(path, NULL);
}
#else
#include <sys/stat.h>
#include <unistd.h>
#endif // WIN32

TestMain("ZeroTier Transport", {

	// trantest_test_all("tcp://127.0.0.1:%u");
	char     path1[NNG_MAXADDRLEN] = "/tmp/zt_server";
	char     path2[NNG_MAXADDRLEN] = "/tmp/zt_client";
	unsigned port;

	port = 5555;

	atexit(nng_fini);

	Convey("We can register the zero tier transport",
	    { So(nng_zt_register() == 0); });

	Convey("We can create a zt listener", {
		nng_listener l;
		nng_socket   s;
		char         addr[NNG_MAXADDRLEN];
		int          rv;

		snprintf(addr, sizeof(addr), "zt://" NWID ":%u", port);

		So(nng_pair_open(&s) == 0);
		Reset({ nng_close(s); });

		So(nng_listener_create(&l, s, addr) == 0);

		Convey("We can lookup zerotier home option id", {
			So(nng_optid_zt_home > 0);
			So(nng_option_lookup(nng_opt_zt_home) ==
			    nng_optid_zt_home);
		});

		Convey("And it can be started...", {

			mkdir(path1, 0700);

			So(nng_listener_setopt(l, nng_optid_zt_home, path1,
			       strlen(path1) + 1) == 0);

			So(nng_listener_start(l, 0) == 0);
		})
	});

	Convey("We can create a zt dialer", {
		nng_dialer d;
		nng_socket s;
		char       addr[NNG_MAXADDRLEN];
		int        rv;
		// uint64_t   node = 0xb000072fa6ull; // my personal host
		uint64_t node = 0x2d2f619cccull; // my personal host

		snprintf(
		    addr, sizeof(addr), "zt://" NWID "/%llx:%u", node, port);

		So(nng_pair_open(&s) == 0);
		Reset({ nng_close(s); });

		So(nng_dialer_create(&d, s, addr) == 0);

		Convey("We can lookup zerotier home option id", {
			So(nng_optid_zt_home > 0);
			So(nng_option_lookup(nng_opt_zt_home) ==
			    nng_optid_zt_home);
		});
	});

	Convey("We can create an ephemeral listener", {
		nng_dialer   d;
		nng_listener l;
		nng_socket   s;
		char         addr[NNG_MAXADDRLEN];
		int          rv;
		uint64_t     node1 = 0;
		uint64_t     node2 = 0;

		snprintf(addr, sizeof(addr), "zt://" NWID ":%u", port);

		So(nng_pair_open(&s) == 0);
		Reset({ nng_close(s); });

		So(nng_listener_create(&l, s, addr) == 0);

		So(nng_listener_getopt_usec(l, nng_optid_zt_node, &node1) ==
		    0);
		So(node1 != 0);

		Convey("Network name option works", {
			char   name[NNG_MAXADDRLEN];
			size_t namesz;

			namesz = sizeof(name);
			So(nng_listener_getopt(l, nng_optid_zt_network_name,
			       name, &namesz) == 0);
			printf("*** NAME IS [%s]\n", name);
		});
		Convey("Connection refused works", {
			snprintf(addr, sizeof(addr), "zt://" NWID "/%llx:%u",
			    node1, 42);
			So(nng_dialer_create(&d, s, addr) == 0);
			So(nng_dialer_getopt_usec(
			       d, nng_optid_zt_node, &node2) == 0);
			So(node2 == node1);
			So(nng_dialer_start(d, 0) == NNG_ECONNREFUSED);
		});
	});

	Convey("We can create a zt pair (dialer & listener)", {
		nng_dialer   d;
		nng_listener l;
		nng_socket   s1;
		nng_socket   s2;
		char         addr1[NNG_MAXADDRLEN];
		char         addr2[NNG_MAXADDRLEN];
		int          rv;
		uint64_t     node;

		port = 9944;
		// uint64_t   node = 0xb000072fa6ull; // my personal host

		snprintf(addr1, sizeof(addr1), "zt://" NWID ":%u", port);

		//		    addr, sizeof(addr), "zt://" NWID
		//"/%llx:%u",  node, port);

		So(nng_pair_open(&s1) == 0);
		So(nng_pair_open(&s2) == 0);
		Reset({
			nng_close(s1);
			// This sleep allows us to ensure disconnect
			// messages work.
			nng_usleep(1000000);
			nng_close(s2);
		});

		So(nng_listener_create(&l, s1, addr1) == 0);
		So(nng_listener_setopt(
		       l, nng_optid_zt_home, path1, strlen(path1) + 1) == 0);

		So(nng_listener_start(l, 0) == 0);
		node = 0;
		So(nng_listener_getopt_usec(l, nng_optid_zt_node, &node) == 0);
		So(node != 0);

		snprintf(
		    addr2, sizeof(addr2), "zt://" NWID "/%llx:%u", node, port);
		So(nng_dialer_create(&d, s2, addr2) == 0);
		So(nng_dialer_setopt(
		       d, nng_optid_zt_home, path2, strlen(path2) + 1) == 0);
		So(nng_dialer_start(d, 0) == 0);

	});

	trantest_test_all("zt://" NWID "/*:%u");

})
