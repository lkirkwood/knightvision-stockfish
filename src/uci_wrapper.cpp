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
JNIEXPORT jlong JNICALL Java_com_knightvision_StockfishBridge__1initEngine(JNIEnv* env, jobject) {
    Stockfish::Bitboards::init();
    Stockfish::Position::init();

    char  progname[] = "stockfish";
    char* argv[]     = {progname, nullptr};
    auto  uci        = new Stockfish::UCIEngine(1, argv);

    Stockfish::Tune::init(uci->engine_options());

    return reinterpret_cast<jlong>(uci);
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

JNIEXPORT jstring JNICALL Java_com_knightvision_StockfishBridge_goBlocking(JNIEnv* env,
                                                                           jobject,
                                                                           jlong _uci,
                                                                           jint  depth) {
    Stockfish::UCIEngine* uci = reinterpret_cast<Stockfish::UCIEngine*>(_uci);

    std::function<void()> run_cmd = [&]() {
        uci->run_cmd("go depth " + std::to_string(depth));
        uci->wait_for_search();
    };

    std::string output = capture_stdout(run_cmd);

    return env->NewStringUTF(output.c_str());
}

JNIEXPORT jboolean JNICALL Java_com_knightvision_StockfishBridge_validFen(JNIEnv* env,
                                                                          jobject,
                                                                          jstring fen_jstring) {

    const char* fen_utf8 = env->GetStringUTFChars(fen_jstring, nullptr);
    std::string fen(fen_utf8);
    env->ReleaseStringUTFChars(fen_jstring, fen_utf8);
    Stockfish::Position  pos;
    Stockfish::StateInfo st;
    pos.set(fen, false, &st);
    return static_cast<jboolean>(pos.pos_is_ok());
}
}
