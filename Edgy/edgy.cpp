#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <exception>
#include <deque>
#include <sstream>
#include <stdlib.h>
#include <queue>
#include <functional>
#include <unordered_set>

#include <zlib.h>

#include <boost/dynamic_bitset.hpp>

class Board {
private:
    std::vector<boost::dynamic_bitset<>> rows;
    size_t playerRow;
    size_t playerCol;
public:
    Board() : playerRow(0), playerCol(0) {};
    template<class T>
    Board(const T& copy, size_t playerRow, size_t playerCol) : rows(copy.size(), boost::dynamic_bitset<>(copy.empty() ? 0 : copy[0].size())), playerRow(playerRow), playerCol(playerCol) {
        for(size_t row=0; row<copy.size(); ++row) {
            for(size_t col=0; col<copy[row].size(); ++col) {
                rows[row][col] = copy[row][col];
            }
        }
    }
    Board(size_t height, size_t width, size_t playerRow, size_t playerCol) : rows(height, boost::dynamic_bitset<>(width)), playerRow(playerRow), playerCol(playerCol) {}
    ~Board() {}

    inline void setMine(size_t row, size_t col) {
        rows[row].set(col, 1);
    }

    inline size_t getPlayerRow() const { return playerRow; }
    inline size_t getPlayerCol() const { return playerCol; }
    inline size_t numRows() const { return rows.size(); }
    inline size_t numCols() const { return rows.empty() ? 0 : rows[0].size(); }
    inline bool hasMine(size_t row, size_t col) const {
        return rows[row][col];
    }
    Board orShift(ssize_t rowDelta, ssize_t colDelta) const {
        Board shifted(*this);
        for(size_t row=0; row<numRows(); ++row) {
            ssize_t newRow = static_cast<ssize_t>(row) + rowDelta;
            if(newRow < 0 || static_cast<size_t>(newRow) >= numRows()) {
                continue;
            }
            if(colDelta < 0) {
                shifted.rows[row] |= (rows[newRow] << (-colDelta));
            } else {
                shifted.rows[row] |= (rows[newRow] >> colDelta);
            }
        }
        return shifted;
    }

    template <class C>
    static Board parse(C& client);
};

std::ostream& operator<<(std::ostream& stream, const Board& board) {
    for(size_t i=0; i<board.numCols() + 2; ++i) {
        stream << '-';
    }
    stream << std::endl;
    for(size_t row=0; row<board.numRows(); ++row) {
        stream << '|';
        for(size_t col=0; col<board.numCols(); ++col) {
            if(board.getPlayerRow() == row && board.getPlayerCol() == col) {
                stream << '@';
            } else if(board.hasMine(row, col)) {
                stream << 'x';
            } else {
                stream << ' ';
            }
        }
        stream << '|' << std::endl;
    }
    for(size_t i=0; i<board.numCols() + 2; ++i) {
        stream << '-';
    }
    return stream;
}

int inflate(const void *src, int srcLen, void *dst, int dstLen) {
    z_stream strm;
    strm.total_in  = strm.avail_in  = srcLen;
    strm.total_out = strm.avail_out = dstLen;
    strm.next_in   = (Bytef *) src;
    strm.next_out  = (Bytef *) dst;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    int err = -1;
    int ret = -1;

    err = inflateInit2(&strm, (15 + 32)); //15 window bits, and the +32 tells zlib to to detect if using gzip or zlib
    if (err == Z_OK) {
        err = inflate(&strm, Z_FINISH);
        if (err == Z_STREAM_END) {
            ret = strm.total_out;
        }
        else {
             inflateEnd(&strm);
             return err;
        }
    }
    else {
        inflateEnd(&strm);
        return err;
    }

    inflateEnd(&strm);
    return ret;
}

class Client {
private:
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    int socketfd;
    std::deque<char> buffer;
    bool silent;
public:
    Client(const char* host, const char* port) : silent(false) {
        memset(&host_info, 0, sizeof host_info);
        host_info.ai_family = AF_UNSPEC;
        host_info.ai_socktype = SOCK_STREAM;
        auto status = getaddrinfo(host, port, &host_info, &host_info_list);
        if(status != 0) {
            throw std::runtime_error(gai_strerror(status));
        }
        socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
        if(socketfd == -1) {
            throw std::runtime_error("socket error!");
        }
        //struct KeepConfig cfg = { 60, 5, 5};
        //set_tcp_keepalive_cfg(socketfd, &cfg);
        //int val = 1;
        //setsockopt(socketfd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof val);
        status = connect(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
        if(status == -1) {
            throw std::runtime_error("connection error!");
        }
    }
    ~Client() {
        freeaddrinfo(host_info_list);
        close(socketfd);
    }
    ssize_t send(const std::string& bytes) {
        return ::send(socketfd, bytes.c_str(), bytes.length(), 0);
    }
    bool hasMoreBytes() {
        if(buffer.empty()) {
            char newData[1024];
            auto bytesReceived = recv(socketfd, newData, std::end(newData) - std::begin(newData), 0);
            if(bytesReceived == 0) {
                throw std::runtime_error("Connection closed by server!");
            } else if(bytesReceived < 0) {
                throw std::runtime_error("I/O Error!");
            }
            for(ssize_t i=0; i<bytesReceived; ++i) {
                buffer.emplace_back(newData[i]);
                if(!silent) {
                    std::cout << newData[i];
                }
            }
        }
        return !buffer.empty();
    }
    int getc() {
        if(!hasMoreBytes()) {
            return -1;
        } else {
            auto ret = buffer.front();
            buffer.pop_front();
            return static_cast<unsigned char>(ret);
        }
    }
    std::string readline() {
        std::stringstream ss;
        while(hasMoreBytes()) {
            char c = buffer.front();
            buffer.pop_front();
            if(c == '\n') {
                break;
            } else {
                ss << c;
            }
        }
        // Check for something like: Sending 141652 bytes of zlib data. I still expect the answer in text though
        auto ret = ss.str();
        if(ret.find("zlib data") != std::string::npos) {
            size_t numBytes = 0;
            const char* offset = ret.c_str();
            while((*offset < '0' || *offset > '9') && *offset != '\0') {
                ++offset;
            }
            if(*offset != '\0') {
                numBytes = atoll(offset);
            }
            std::cout << "Receiving " << numBytes << " of zlib data..." << std::endl;
            size_t destSize = numBytes * 4;
            char* compressed = new char[numBytes];
            auto oldSilent = silent;
            silent = true;
            for(size_t i=0; i<numBytes; ++i) {
                int c = getc();
                if(c >= 0) {
                    compressed[i] = static_cast<char>(c);
                }
            }
            silent = oldSilent;
            for(;;) {
                char* expanded = new char[destSize];
                auto ret = inflate(compressed, numBytes, expanded, destSize);
                if(ret == Z_BUF_ERROR) {
                    destSize *= 2;
                    delete expanded;
                    continue;
                } else if(ret == Z_DATA_ERROR) {
                    delete expanded;
                    delete compressed;
                    throw new std::runtime_error("Corrupt zlib data!");
                } else if(ret == Z_MEM_ERROR) {
                    delete expanded;
                    delete compressed;
                    throw new std::runtime_error("Not enough memory to decompress the zlib data!");
                } else {
                    destSize = ret;
                    std::cout << "zlib data decompressed to " << destSize << " bytes!" << std::endl;
                    for(ssize_t i=destSize - 1; i>=0; --i) {
                        buffer.push_front(expanded[i]);
                    }
                    delete expanded;
                    break;
                }
            }
            delete compressed;
        }
        return ret;
    }
};

template <class C>
Board Board::parse(C& client) {
    size_t width = 0;
    
    size_t x = 0;
    size_t y = 0;

    std::vector<std::vector<bool>> board;

    while(client.hasMoreBytes()) {
        auto line = client.readline();

        if(width == 0) {
            for(auto c : line) {
                if(c == '-') {
                    ++width;
                } else if(c != '\n') {
                    width = 0;
                    break;
                }
            }
            if(width > 2) {
                width -= 2;
            }
        } else {
            if(line[0] == '-') {
                break;
            }
            std::vector<bool> row(width, false);
            for(size_t i=0; i<line.length(); ++i) {
                if(i > 0 && i - 1 >= width) {
                    break;
                } else if(line[i] == 'x') {
                    row[i-1] = true;
                } else if(line[i] == '@') {
                    y = board.size();
                    x = i - 1;
                }
            }
            board.push_back(std::move(row));
        }
    }

    return Board(board, y, x);
}

template <typename L, typename R>
inline L absDiff(L lhs, R rhs) {
    return lhs > rhs ? lhs - rhs : rhs - lhs;
}

typedef struct tagSearchNode {
    size_t row;
    size_t col;
    std::string path;
    inline size_t fCost(size_t goalRow, size_t goalCol) const {
        return path.length() + absDiff(goalRow, row) + absDiff(goalCol, col);
    }
    tagSearchNode(size_t row, size_t col, std::string path) : row(row), col(col), path(path) {}
} SearchNode;

std::string astar(const Board& board, size_t goalRow, size_t goalCol, size_t maxMoves) {
    typedef std::priority_queue<SearchNode, std::vector<SearchNode>, std::function<bool(const SearchNode&,const SearchNode&)>> QueueType;
    
    QueueType queue([goalRow,goalCol](const SearchNode& lhs, const SearchNode& rhs) -> bool {
            return lhs.fCost(goalRow,goalCol) > rhs.fCost(goalRow,goalCol);
        });

    queue.emplace(board.getPlayerRow(), board.getPlayerCol(), "");

    std::unordered_set<size_t> history;

    while(!queue.empty()) {
        SearchNode n = std::move(queue.top());
        queue.pop();
        if(!history.insert(n.row * board.numCols() + n.col).second) {
            /* we have already visited this state! */
            continue;
        }
        //std::cerr << "Searching node " << n.row << ", " << n.col << ", " << n.fCost(goalRow, goalCol) << "\t" << n.path << std::endl;
        if(n.row == goalRow && n.col == goalCol) {
            return n.path;
        }
        if(n.path.length() < maxMoves) {
            if(n.row > 0 && !board.hasMine(n.row - 1, n.col)) {
                queue.emplace(n.row - 1, n.col, n.path + "W");
            }
            if(n.col > 0 && !board.hasMine(n.row, n.col - 1)) {
                queue.emplace(n.row, n.col - 1, n.path + "A");
            }
            if(n.row < board.numRows() - 1 && !board.hasMine(n.row + 1, n.col)) {
                queue.emplace(n.row + 1, n.col, n.path + "S");
            }
            if(n.col < board.numCols() - 1 && !board.hasMine(n.row, n.col + 1)) {
                queue.emplace(n.row, n.col + 1, n.path + "D");
            }
        }
    }

    return "";
}

std::string solve(const Board& board, size_t maxMoves) {
    //std::cerr << "Solving for board: " << std::endl << board << std::endl << std::endl;
    double percentDone = -1.0;
    ssize_t rangeSize = maxMoves * 2 + 1;
    //size_t counter = 0;
    for(ssize_t rowDelta=-maxMoves; rowDelta<=static_cast<ssize_t>(maxMoves); ++rowDelta) {
        for(ssize_t colDelta=-maxMoves; colDelta<=static_cast<ssize_t>(maxMoves); ++colDelta) {
            double lastPercent = percentDone;
            percentDone = double(size_t(double((rowDelta + maxMoves) * rangeSize + (colDelta + maxMoves)) / double(rangeSize * rangeSize) * 1000.0 + 0.5)) / 10.0;
            if(percentDone > lastPercent) {
                std::cerr << "\x1b[2K\r" << "Solving for up to " << maxMoves << " moves... " << percentDone << "%";
            }
            if(static_cast<size_t>(absDiff(rowDelta, 0) + absDiff(colDelta, 0)) > maxMoves || (rowDelta == 0 && colDelta == 0)) {
                continue;
            }
            size_t rowShiftsRequired = 1;
            if(rowDelta < 0) {
                rowShiftsRequired += board.getPlayerRow() / -rowDelta;
            } else if(rowDelta != 0) {
                rowShiftsRequired += (board.numRows() - board.getPlayerRow()) / rowDelta;
            }
            size_t colShiftsRequired = 1;
            if(colDelta < 0) {
                colShiftsRequired += board.getPlayerCol() / -colDelta;
            } else if(colDelta != 0) {
                colShiftsRequired += (board.numCols() - board.getPlayerCol()) / colDelta;
            }
            size_t shiftsRequired = std::max(rowShiftsRequired, colShiftsRequired);
            //std::cerr << "Shifts required for delta " << rowDelta << ", " << colDelta << ": " << shiftsRequired << std::endl;
            Board shifted = board.orShift(rowDelta, colDelta);
            for(size_t i=1; i<shiftsRequired; ++i) {
                shifted = shifted.orShift(rowDelta, colDelta);
            }
            //std::cerr << "Shifted board: " << std::endl << shifted << std::endl << std::endl;
            if(shifted.hasMine(shifted.getPlayerRow(), shifted.getPlayerCol())) {
                continue;
            }
            auto solution = astar(shifted, static_cast<ssize_t>(board.getPlayerRow()) + rowDelta, static_cast<ssize_t>(board.getPlayerCol()) + colDelta, maxMoves);
            if(solution.length() > 0) {
                std::cerr << "\x1b[2K\r";
                return solution;
            }
        }
    }
    std::cerr << "\x1b[2K\r";
    return "";
}

int play(const char* host, const char* port) {
    Client client(host, port);
    if(client.readline() != "Password") {
        return 1;
    }
    client.send("EdgesAreFun\n");
    size_t gameNumber = 0;
    while(client.hasMoreBytes()) {
        auto board = Board::parse(client);
        if(board.numRows() == 0) {
            break;
        }
        ++gameNumber;
        std::cout << "Game #" << gameNumber << std::endl;
        auto numMovesLine = client.readline();
        size_t maxMoves = 6;
        const char* moveIntOffset = numMovesLine.c_str();
        while((*moveIntOffset < '0' || *moveIntOffset > '9') && *moveIntOffset != '\0') {
            ++moveIntOffset;
        }
        if(*moveIntOffset != '\0') {
            maxMoves = atoll(moveIntOffset);
        }
        auto solution = solve(board, maxMoves);
        std::cout << solution << std::endl;
        client.send(solution + "\n");
    }
    return 0;
}

int main(int, char**) {
    return play("edgy.2015.ghostintheshellcode.com", "44440");
}
