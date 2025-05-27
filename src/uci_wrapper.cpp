#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "tune.h"
#include "uci.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <jni.h>
#include <ostream>
#include <sstream>
#include <unistd.h>

const std::string capture_stdout(const std::function<void()>& func) {
    int original_stdout = dup(STDOUT_FILENO);

    int pipefd[2];
    pipe(pipefd);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    func();

    fflush(stdout);
    dup2(original_stdout, STDOUT_FILENO);
    close(original_stdout);

    std::string output{};
    char        buf[4096];
    ssize_t     count;
    while ((count = read(pipefd[0], buf, sizeof(buf))) > 0)
    {
        output.append(buf, count);
    }
    close(pipefd[0]);

    return output;
}

extern "C" {
JNIEXPORT jlongArray JNICALL Java_com_knightvision_StockfishBridge__1initEngine(JNIEnv* env,
                                                                                jobject) {
    Stockfish::Bitboards::init();
    Stockfish::Position::init();

    char                progname[]      = "stockfish";
    char*               argv[]          = {progname, nullptr};
    std::ostringstream* bestmove_output = new std::ostringstream();
    auto                uci             = new Stockfish::UCIEngine(1, argv, bestmove_output);

    Stockfish::Tune::init(uci->engine_options());

    jlong native_ptr_array[] = {reinterpret_cast<jlong>(uci),
                                reinterpret_cast<jlong>(bestmove_output)};

    jlongArray jni_ptr_array = env->NewLongArray(2);
    if (NULL == jni_ptr_array)
        return NULL;

    env->SetLongArrayRegion(jni_ptr_array, 0, 2, native_ptr_array);
    return jni_ptr_array;
}

JNIEXPORT jstring JNICALL Java_com_knightvision_StockfishBridge_runCmd(JNIEnv* env,
                                                                       jobject,
                                                                       jlong   uci_handle,
                                                                       jstring cmd_jstring) {

    auto* uci = reinterpret_cast<Stockfish::UCIEngine*>(uci_handle);

    const char* cmd_utf8 = env->GetStringUTFChars(cmd_jstring, nullptr);
    std::string cmd(cmd_utf8);
    env->ReleaseStringUTFChars(cmd_jstring, cmd_utf8);

    std::function<void()> run_cmd = [&]() { uci->run_cmd(cmd); };
    std::string           output  = capture_stdout(run_cmd);

    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jstring JNICALL Java_com_knightvision_StockfishBridge_goBlocking(
  JNIEnv* env, jobject, jlong _uci, jlong _bestmove_output, jint depth) {
    Stockfish::UCIEngine* uci             = reinterpret_cast<Stockfish::UCIEngine*>(_uci);
    std::ostringstream*   bestmove_output = reinterpret_cast<std::ostringstream*>(_bestmove_output);

    std::function<void()> run_cmd = [&]() {
        uci->run_cmd("go depth " + std::to_string(depth));
        uci->wait_for_search();
    };

    std::string output = capture_stdout(run_cmd);
    uci->await_bestmove();

    return env->NewStringUTF((output + "\n" + bestmove_output->str()).c_str());
}

JNIEXPORT jstring JNICALL Java_com_knightvision_StockfishBridge_bestmove(JNIEnv* env,
                                                                         jobject,
                                                                         jlong _uci,
                                                                         jlong _bestmove_output) {
    Stockfish::UCIEngine* uci             = reinterpret_cast<Stockfish::UCIEngine*>(_uci);
    std::ostringstream*   bestmove_output = reinterpret_cast<std::ostringstream*>(_bestmove_output);

    uci->run_cmd("stop");
    uci->await_bestmove();

    return env->NewStringUTF(bestmove_output->str().c_str());
}
}
