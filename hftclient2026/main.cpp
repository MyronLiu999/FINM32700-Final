#include <iostream>
#include <string>
#include <vector>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/tcp.h>

using namespace std;

static const int MOD = 997;
static const size_t BUF_SIZE = 1 << 20;   // 1 MB read buffer

class FastSocketInput {
private:
    int sock_;
    vector<char> buf_;
    size_t pos_ = 0;
    size_t end_ = 0;

    bool refill() {
        pos_ = 0;
        ssize_t n = recv(sock_, buf_.data(), buf_.size(), 0);
        if (n <= 0) return false;
        end_ = (size_t)n;
        return true;
    }

    inline bool getChar(char& c) {
        if (pos_ >= end_) {
            if (!refill()) return false;
        }
        c = buf_[pos_++];
        return true;
    }

public:
    explicit FastSocketInput(int sock) : sock_(sock), buf_(BUF_SIZE) {}

    bool readInt(int& out) {
        char c;
        do {
            if (!getChar(c)) return false;
        } while (c <= ' ');

        int sign = 1;
        if (c == '-') {
            sign = -1;
            if (!getChar(c)) return false;
        }

        int x = 0;
        while (c > ' ') {
            x = x * 10 + (c - '0');
            if (!getChar(c)) break;
        }

        out = x * sign;
        return true;
    }
};

static bool sendAll(int sock, const char* msg, size_t len) {
    const char* p = msg;
    size_t left = len;

    while (left > 0) {
        ssize_t n = send(sock, p, left, MSG_NOSIGNAL);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += n;
        left -= (size_t)n;
    }
    return true;
}

static bool sendAll(int sock, const string& msg) {
    return sendAll(sock, msg.c_str(), msg.size());
}

static int connectToServer(const string& host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    int bufSize = 4 * 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        cerr << "Bad host address: " << host << "\n";
        close(sock);
        return -1;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

static bool solveOneChallenge(FastSocketInput& in, vector<int>& colSumA, int& challengeId, int& answer) {
    int N;
    if (!in.readInt(challengeId)) return false;
    if (!in.readInt(N)) return false;

    colSumA.assign(N, 0);

    int x;

    // Read A and compute raw column sums. Modulo is delayed to avoid work in the hot loop.
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            if (!in.readInt(x)) return false;
            colSumA[j] += x;
        }
    }

    // Read each B row, then immediately add its contribution to the checksum.
    int ans = 0;
    for (int i = 0; i < N; ++i) {
        int row = 0;
        for (int j = 0; j < N; ++j) {
            if (!in.readInt(x)) return false;
            row += x;
        }
        ans = (ans + (int)(((long long)(colSumA[i] % MOD) * (row % MOD)) % MOD)) % MOD;
    }

    answer = ans;
    return true;
}


int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <host> <port> \n";
        return 1;
    }

    string host = argv[1];
    int port = stoi(argv[2]);
    string team = "Team Moxiao Li and Myron liu";

    int sock = connectToServer(host, port);
    if (sock < 0) return 1;

    if (!sendAll(sock, team + "\n")) {
        cerr << "Failed to send team name.\n";
        close(sock);
        return 1;
    }

    cerr << "Connected as " << team << " to " << host << ":" << port << "\n";

    FastSocketInput in(sock);
    vector<int> colSumA;
    char reply[64];

    while (true) {
        int cid = 0;
        int ans = 0;

        if (!solveOneChallenge(in, colSumA, cid, ans)) {
            cerr << "Server disconnected or incomplete challenge received.\n";
            break;
        }

        int replyLen = snprintf(reply, sizeof(reply), "%d %d\n", cid, ans);
        if (replyLen <= 0 || !sendAll(sock, reply, (size_t)replyLen)) {
            cerr << "Failed to send answer.\n";
            break;
        }
    }

    close(sock);
    return 0;
}
