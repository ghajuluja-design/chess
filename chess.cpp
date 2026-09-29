// Terminal chess game for two players (hotseat).
// Move format: e2e4, promotion: e7e8q. Commands: resign, draw, undo, quit.
#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <optional>

enum class Color { White, Black };

struct Piece {
    char type;   // p n b r q k
    Color color;
};

struct Move {
    int from, to;
    char promo = 0;
    bool isEnPassant = false;
    bool isCastle = false;
    std::optional<Piece> captured;
    int capturedSq = -1;
};

struct GameState {
    std::optional<Piece> board[64];
    Color turn = Color::White;
    bool wK = true, wQ = true, bK = true, bQ = true;  // castling rights
    int epSquare = -1;
    int halfmove = 0, fullmove = 1;
};

constexpr int sq(int file, int rank) { return rank * 8 + file; }  // rank 0 = rank 1 (white side)
inline int fileOf(int s) { return s % 8; }
inline int rankOf(int s) { return s / 8; }
inline Color other(Color c) { return c == Color::White ? Color::Black : Color::White; }

const char* GLYPH_W[] = {"\xe2\x99\x94", "\xe2\x99\x95", "\xe2\x99\x96", "\xe2\x99\x97", "\xe2\x99\x98", "\xe2\x99\x99"}; // K Q R B N P
const char* GLYPH_B[] = {"\xe2\x99\x9a", "\xe2\x99\x9b", "\xe2\x99\x9c", "\xe2\x99\x9d", "\xe2\x99\x9e", "\xe2\x99\x9f"};

const char* glyph(Piece p) {
    const char* idx = "kqrbnp";
    int i = 0;
    for (int k = 0; idx[k]; k++) if (idx[k] == p.type) { i = k; break; }
    return p.color == Color::White ? GLYPH_W[i] : GLYPH_B[i];
}

GameState initialPosition() {
    GameState g;
    const char back[] = "rnbqkbnr";
    for (int f = 0; f < 8; f++) {
        g.board[sq(f, 0)] = Piece{back[f], Color::White};
        g.board[sq(f, 1)] = Piece{'p', Color::White};
        g.board[sq(f, 6)] = Piece{'p', Color::Black};
        g.board[sq(f, 7)] = Piece{back[f], Color::Black};
    }
    return g;
}

bool onBoard(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }

bool isAttacked(const GameState& g, int target, Color byColor) {
    int tf = fileOf(target), tr = rankOf(target);
    // pawn attacks
    int pdir = byColor == Color::White ? -1 : 1;  // attacker pawn sits one rank "behind" relative to its attack direction
    for (int df : {-1, 1}) {
        int f = tf + df, r = tr + pdir;
        if (onBoard(f, r)) {
            auto p = g.board[sq(f, r)];
            if (p && p->color == byColor && p->type == 'p') return true;
        }
    }
    // knights
    const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
    for (auto& d : kn) {
        int f = tf + d[0], r = tr + d[1];
        if (onBoard(f, r)) {
            auto p = g.board[sq(f, r)];
            if (p && p->color == byColor && p->type == 'n') return true;
        }
    }
    // king
    for (int df = -1; df <= 1; df++) for (int dr = -1; dr <= 1; dr++) {
        if (!df && !dr) continue;
        int f = tf + df, r = tr + dr;
        if (onBoard(f, r)) {
            auto p = g.board[sq(f, r)];
            if (p && p->color == byColor && p->type == 'k') return true;
        }
    }
    // sliders
    const int bish[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    const int rook[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (auto& d : bish) {
        int f = tf + d[0], r = tr + d[1];
        while (onBoard(f, r)) {
            auto p = g.board[sq(f, r)];
            if (p) {
                if (p->color == byColor && (p->type == 'b' || p->type == 'q')) return true;
                break;
            }
            f += d[0]; r += d[1];
        }
    }
    for (auto& d : rook) {
        int f = tf + d[0], r = tr + d[1];
        while (onBoard(f, r)) {
            auto p = g.board[sq(f, r)];
            if (p) {
                if (p->color == byColor && (p->type == 'r' || p->type == 'q')) return true;
                break;
            }
            f += d[0]; r += d[1];
        }
    }
    return false;
}

int findKing(const GameState& g, Color c) {
    for (int s = 0; s < 64; s++)
        if (g.board[s] && g.board[s]->type == 'k' && g.board[s]->color == c) return s;
    return -1;
}

bool inCheck(const GameState& g, Color c) {
    int k = findKing(g, c);
    return k >= 0 && isAttacked(g, k, other(c));
}

// Pseudo-legal move generation for one side.
std::vector<Move> pseudoMoves(const GameState& g, Color c) {
    std::vector<Move> moves;
    auto add = [&](int from, int to, char promo = 0, bool ep = false, bool castle = false) {
        Move m{from, to, promo, ep, castle};
        if (g.board[to]) { m.captured = g.board[to]; m.capturedSq = to; }
        else if (ep) { m.captured = g.board[sq(fileOf(to), rankOf(from))]; m.capturedSq = sq(fileOf(to), rankOf(from)); }
        moves.push_back(m);
    };
    for (int s = 0; s < 64; s++) {
        auto p = g.board[s];
        if (!p || p->color != c) continue;
        int f = fileOf(s), r = rankOf(s);
        if (p->type == 'p') {
            int dir = c == Color::White ? 1 : -1;
            int promoRank = c == Color::White ? 7 : 0;
            if (onBoard(f, r + dir) && !g.board[sq(f, r + dir)]) {
                if (r + dir == promoRank) for (char pr : {'q','r','b','n'}) add(s, sq(f, r + dir), pr);
                else add(s, sq(f, r + dir));
                int startRank = c == Color::White ? 1 : 6;
                if (r == startRank && !g.board[sq(f, r + 2 * dir)])
                    add(s, sq(f, r + 2 * dir));
            }
            for (int df : {-1, 1}) {
                int nf = f + df, nr = r + dir;
                if (!onBoard(nf, nr)) continue;
                int t = sq(nf, nr);
                if (g.board[t] && g.board[t]->color != c) {
                    if (nr == promoRank) for (char pr : {'q','r','b','n'}) add(s, t, pr);
                    else add(s, t);
                } else if (!g.board[t] && t == g.epSquare) {
                    add(s, t, 0, true);
                }
            }
        } else if (p->type == 'n' || p->type == 'k') {
            const int (*dirs)[2] = nullptr;
            const int kn[8][2] = {{1,2},{2,1},{-1,2},{-2,1},{1,-2},{2,-1},{-1,-2},{-2,-1}};
            const int kg[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
            int n = 8;
            if (p->type == 'n') dirs = kn; else dirs = kg;
            for (int i = 0; i < n; i++) {
                int nf = f + dirs[i][0], nr = r + dirs[i][1];
                if (!onBoard(nf, nr)) continue;
                int t = sq(nf, nr);
                if (!g.board[t] || g.board[t]->color != c) add(s, t);
            }
            if (p->type == 'k') {
                bool rightsK = c == Color::White ? g.wK : g.bK;
                bool rightsQ = c == Color::White ? g.wQ : g.bQ;
                int home = c == Color::White ? 0 : 7;
                if (r == home && f == 4 && !inCheck(g, c)) {
                    if (rightsK && !g.board[sq(5, home)] && !g.board[sq(6, home)]
                        && g.board[sq(7, home)] && g.board[sq(7, home)]->type == 'r' && g.board[sq(7, home)]->color == c
                        && !isAttacked(g, sq(5, home), other(c)) && !isAttacked(g, sq(6, home), other(c)))
                        add(s, sq(6, home), 0, false, true);
                    if (rightsQ && !g.board[sq(3, home)] && !g.board[sq(2, home)] && !g.board[sq(1, home)]
                        && g.board[sq(0, home)] && g.board[sq(0, home)]->type == 'r' && g.board[sq(0, home)]->color == c
                        && !isAttacked(g, sq(3, home), other(c)) && !isAttacked(g, sq(2, home), other(c)))
                        add(s, sq(2, home), 0, false, true);
                }
            }
        } else {  // sliders b r q
            const int bish[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
            const int rook[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
            auto slide = [&](const int (*d)[2]) {
                for (int i = 0; i < 4; i++) {
                    int nf = f + d[i][0], nr = r + d[i][1];
                    while (onBoard(nf, nr)) {
                        int t = sq(nf, nr);
                        if (!g.board[t]) add(s, t);
                        else { if (g.board[t]->color != c) add(s, t); break; }
                        nf += d[i][0]; nr += d[i][1];
                    }
                }
            };
            if (p->type == 'b' || p->type == 'q') slide(bish);
            if (p->type == 'r' || p->type == 'q') slide(rook);
        }
    }
    return moves;
}

void applyMove(GameState& g, const Move& m) {
    Piece mover = *g.board[m.from];
    g.board[m.from].reset();
    if (m.captured) g.board[m.capturedSq].reset();
    g.board[m.to] = Piece{m.promo ? m.promo : mover.type, mover.color};

    if (m.isCastle) {
        int home = rankOf(m.from);
        if (fileOf(m.to) == 6) {  // kingside
            g.board[sq(5, home)] = g.board[sq(7, home)];
            g.board[sq(7, home)].reset();
        } else {                  // queenside
            g.board[sq(3, home)] = g.board[sq(0, home)];
            g.board[sq(0, home)].reset();
        }
    }
    // en passant target
    g.epSquare = -1;
    if (mover.type == 'p' && rankOf(m.to) - rankOf(m.from) == 2) g.epSquare = sq(fileOf(m.from), rankOf(m.from) + 1);
    if (mover.type == 'p' && rankOf(m.from) - rankOf(m.to) == 2) g.epSquare = sq(fileOf(m.from), rankOf(m.from) - 1);

    // castling rights
    if (mover.type == 'k') {
        if (mover.color == Color::White) { g.wK = g.wQ = false; }
        else { g.bK = g.bQ = false; }
    }
    if (m.from == sq(0, 0) || m.to == sq(0, 0)) g.wQ = false;
    if (m.from == sq(7, 0) || m.to == sq(7, 0)) g.wK = false;
    if (m.from == sq(0, 7) || m.to == sq(0, 7)) g.bQ = false;
    if (m.from == sq(7, 7) || m.to == sq(7, 7)) g.bK = false;

    if (mover.type == 'p' || m.captured) g.halfmove = 0; else g.halfmove++;
    if (g.turn == Color::Black) g.fullmove++;
    g.turn = other(g.turn);
}

std::vector<Move> legalMoves(const GameState& g, Color c) {
    std::vector<Move> result;
    for (auto& m : pseudoMoves(g, c)) {
        GameState copy = g;
        applyMove(copy, m);
        if (!inCheck(copy, c)) result.push_back(m);
    }
    return result;
}

std::string sqName(int s) {
    return std::string{char('a' + fileOf(s)), char('1' + rankOf(s))};
}

void printBoard(const GameState& g) {
    std::cout << "\n";
    for (int r = 7; r >= 0; r--) {
        std::cout << "  " << (r + 1) << " ";
        for (int f = 0; f < 8; f++) {
            auto p = g.board[sq(f, r)];
            std::cout << (p ? glyph(*p) : ".") << " ";
        }
        std::cout << "\n";
    }
    std::cout << "    a b c d e f g h\n\n";
}

bool parseSquare(const std::string& s, int& out) {
    if (s.size() != 2) return false;
    if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return false;
    out = sq(s[0] - 'a', s[1] - '1');
    return true;
}

int main() {
#ifdef _WIN32
    system("");  // enable ANSI/UTF-8 on Windows console
#endif
    GameState current = initialPosition();
    std::vector<GameState> history;

    std::cout << "=== Terminal Chess ===\n";
    std::cout << "Moves: e2e4 (promotion: e7e8q). Commands: resign, draw, undo, quit.\n";
    printBoard(current);

    while (true) {
        std::string name = current.turn == Color::White ? "White" : "Black";
        std::cout << name << " to move";
        if (inCheck(current, current.turn)) std::cout << " (CHECK!)";
        std::cout << ": ";

        std::string input;
        if (!std::getline(std::cin, input)) break;

        // trim
        size_t b = input.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = input.find_last_not_of(" \t\r\n");
        input = input.substr(b, e - b + 1);
        for (auto& ch : input) ch = std::tolower(static_cast<unsigned char>(ch));

        if (input == "quit" || input == "exit") { std::cout << "Goodbye.\n"; break; }
        if (input == "undo") {
            if (history.empty()) { std::cout << "Nothing to undo.\n"; continue; }
            current = history.back();
            history.pop_back();
            printBoard(current);
            continue;
        }
        if (input == "resign") {
            std::cout << name << " resigns. " << (current.turn == Color::White ? "Black" : "White") << " wins!\n";
            break;
        }
        if (input == "draw") {
            std::cout << "Draw agreed. Game over.\n";
            break;
        }

        int from, to;
        if (input.size() < 4 || !parseSquare(input.substr(0, 2), from) || !parseSquare(input.substr(2, 2), to)) {
            std::cout << "Invalid input. Use e.g. e2e4 or e7e8q.\n";
            continue;
        }
        char promo = input.size() >= 5 ? input[4] : 0;

        auto legal = legalMoves(current, current.turn);
        const Move* found = nullptr;
        for (auto& m : legal)
            if (m.from == from && m.to == to && (m.promo == promo || (!promo && !m.promo))) { found = &m; break; }
        if (!found && !promo) {  // promotion move without suffix
            for (auto& m : legal)
                if (m.from == from && m.to == to && m.promo == 'q') { found = &m; break; }
        }
        if (!found) {
            // explain why
            if (!current.board[from]) std::cout << "There is no piece on " << input.substr(0, 2) << ".\n";
            else if (current.board[from]->color != current.turn) std::cout << "That is not your piece.\n";
            else std::cout << "Illegal move.\n";
            continue;
        }

        history.push_back(current);
        applyMove(current, *found);
        printBoard(current);

        auto nextLegal = legalMoves(current, current.turn);
        bool check = inCheck(current, current.turn);
        if (nextLegal.empty()) {
            if (check) {
                std::cout << "Checkmate! " << (current.turn == Color::White ? "Black" : "White") << " wins!\n";
            } else {
                std::cout << "Stalemate. Draw.\n";
            }
            break;
        }
        if (current.halfmove >= 100) { std::cout << "Draw by 50-move rule.\n"; break; }
        // insufficient material
        int counts[2] = {0, 0};
        bool hasPawnOrRQ = false;
        for (int s = 0; s < 64; s++)
            if (current.board[s]) {
                counts[int(current.board[s]->color)]++;
                if (current.board[s]->type == 'p' || current.board[s]->type == 'r' || current.board[s]->type == 'q')
                    hasPawnOrRQ = true;
            }
        if (!hasPawnOrRQ && counts[0] <= 2 && counts[1] <= 2) {
            std::cout << "Draw by insufficient material.\n";
            break;
        }
    }
    return 0;
}
