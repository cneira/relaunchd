/*
 * Copyright (c) 2022 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <future>
#include <thread>

#include "common.hpp"
#include "rpc_client.h"

extern int launchctl_main(int argc, char *argv[]);

void testList() {
    TestContext ctx;
    ctx.loadTemporaryManifest({
                                      {"Label", "testList"},
                                      {"Program", "/bin/sh"},
                                      {"RunAtLoad", true}
                              });
    ctx.mgr.startAllJobs();
    std::array<const char *, 2> argv = {"launchctl", "list"};
    std::thread thr{[&argv]() { launchctl_main(argv.size(), (char **)&argv); }};
    ctx.mgr.handleEvent();
    thr.join();
}

void testKill() {
    auto mgrp = testutil::getTemporaryManager();
    auto &mgr = *mgrp;
    json manifest = json::parse(R"(
        {
          "Label": "testKill",
          "ProgramArguments": ["/bin/sh", "-c", "sleep 60"],
          "RunAtLoad": true
        }
    )");
    std::string path = "/dev/null";
    mgr.loadManifest(manifest, path);
    mgr.startAllJobs();
    auto cb = [&mgr]() -> int {
        RpcClient client;
        std::vector<std::string> args = {"15", "testKill"};
        client.invokeMethod("kill", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp = async(std::launch::async, cb);
    mgr.handleEvent();
    assert(fp.get() == 0);
}

void testHelp() {
    std::array<const char *, 2> argv = {"launchctl", "--help"};
    int rv = launchctl_main(argv.size(), (char **)&argv);
    assert(rv == EXIT_SUCCESS);
}

void testUsage() {
    std::array<const char *, 1> argv = {"launchctl"};
    int rv = launchctl_main(argv.size(), (char **)&argv);
    assert(rv == EXIT_FAILURE);
}

void testVersion() {
    auto mgrp = testutil::getTemporaryManager();
    auto &mgr = *mgrp;
    mgr.startAllJobs();
    auto cb = [&mgr]() -> int {
        RpcClient client;
        std::vector<std::string> args;
        client.invokeMethod("version", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp = async(std::launch::async, cb);
    mgr.handleEvent();
    assert(fp.get() == 0);
}

void testSubcommandNotFound() {
    std::array<const char *, 2> argv = {"launchctl", "some-unknown-command"};
    int rv = -1;
    std::thread thr{
        [&argv, &rv]() { rv = launchctl_main(argv.size(), (char **)&argv); }};
    thr.join();
    assert(rv == EXIT_FAILURE);
}

void testLoadAndUnload() {
    std::string label = "testLoadAndUnload";
    auto path = testutil::createManifest(label, json::parse(R"(
        {
          "Label": "testLoadAndUnload",
          "ProgramArguments": ["/bin/sh"],
          "RunAtLoad": true
        }
    )"));
    auto mgrp = testutil::getTemporaryManager();
    auto &mgr = *mgrp;
    mgr.startAllJobs();
    auto cb = [&mgr, &path]() -> int {
        RpcClient client;
        std::vector<std::string> args = {path};
        client.invokeMethod("load", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp = async(std::launch::async, cb);
    mgr.handleEvent();
    assert(mgr.jobExists(label));
    assert(fp.get() == 0);

    // Now unload this job.
    auto cb2 = [&mgr, &path]() -> int {
        RpcClient client;
        std::vector<std::string> args = {path};
        client.invokeMethod("unload", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp2 = async(std::launch::async, cb2);
    mgr.handleEvent();
    mgr.handleEvent();
    assert(fp2.get() == 0);
    if (mgr.jobExists(label)) {
        log_error("unexpected state");
        mgr.dumpJob(label);
    }
    assert(!mgr.jobExists(label));
}

void forceTestLoad(const std::string &path, Manager &mgr) {
    std::future<int> fp = async(std::launch::async,
                                [&path, &mgr]() -> int {
                                    RpcClient client;
                                    std::vector<std::string> args = {"-F", path.c_str()};
                                    client.invokeMethod("load", args, mgr.getDomain());
                                    return 0;
                                });
    mgr.handleEvent();
    assert(fp.get() == 0);
}

void testDisable() {
    std::string label = "testDisable";
    json manifest = {
            {"Label", label},
            {"Program", "/bin/sh"},
    };
    auto path = testutil::createManifest(label, manifest);
    auto mgr = testutil::getTemporaryManager();
    std::future<int> fp = async(std::launch::async,
                           [&label, &mgr]() -> int {
            RpcClient client;
            std::vector<std::string> args = {label};
            client.invokeMethod("disable", args, mgr->getDomain());
            return 0;
        });
    mgr->handleEvent();
    assert(fp.get() == 0);
    mgr->loadManifest(path);
    assert(!mgr->jobExists(label));

    // Try to force load a disabled job
    forceTestLoad(path, *mgr);
    assert(mgr->jobExists(label));
}

void testEnable() {
    std::string label = "testEnable";
    json manifest = {
            {"Label", label},
            {"Program", "/bin/sh"},
            {"Keepalive", true},
            {"Disabled", true},
    };
    auto path = testutil::createManifest(label, manifest);
    auto mgrp = testutil::getTemporaryManager();
    auto &mgr = *mgrp;
    mgr.loadManifest(path, false, true);
    mgr.startAllJobs();
    auto cb = [&mgr, &label]() -> int {
        RpcClient client;
        std::vector<std::string> args = {label};
        client.invokeMethod("enable", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp = async(std::launch::async, cb);
    mgr.handleEvent();
    assert(fp.get() == 0);
    assert(mgr.jobExists(label));
    mgr.dumpJob(label);
    // TODO: check if the job is running. This isn't available via ::Manager yet.
}

void testSubmit() {
    std::string label = "testSubmit";
    auto mgrp = testutil::getTemporaryManager();
    auto &mgr = *mgrp;
    mgr.clearStateFile();
    auto cb = [&mgr, &label]() -> int {
        RpcClient client;
        std::vector<std::string> args = {"submit", "-l", label, "--", "/bin/sh"};
        client.invokeMethod("submit", args, mgr.getDomain());
        return 0;
    };
    std::future<int> fp = async(std::launch::async, cb);
    mgr.handleEvent();
    assert(fp.get() == 0);
    assert(mgr.jobExists(label));
}

void addLaunchctlTests(TestRunner &runner) {
#define X(y) runner.addTest("" #y, y)
    X(testSubmit);
    X(testDisable);
    X(testEnable);
    X(testLoadAndUnload);
    X(testSubcommandNotFound);
    X(testList);
    X(testUsage);
    X(testHelp);
    X(testKill);
    X(testVersion);
#undef X
}
