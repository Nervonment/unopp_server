#ifndef GOMOKU_HPP
#define GOMOKU_HPP

#include <array>
#include <string>
#include <climits>
#include <json/json.h>
#include <cstdlib>
#include <ctime>
#include <random>

template <typename T>
bool equal(const T& a, const T& b) {
    return a == b;
}

template <typename T, typename... Args>
bool equal(const T& a, const T& b, const Args& ... rest) {
    return a == b && equal(b, rest...);
}

class Gomoku {
    struct POINT {
        int x;
        int y;
    };

    inline static constexpr int	board_width = 15;	//	横向路数
    inline static constexpr int	board_height = 15;	//	纵向路数
    std::array<std::array<char, board_width>, board_height> board; // 棋盘
    const char	WHITE_PIECE = 'w';	//	白棋
    const char	BLACK_PIECE = 'b';	//	黑棋
    const char	NONE = 'n';	//	空
    bool		current_is_black = true;		//	现在是否轮到 黑棋
    POINT       last_drop = { -1,-1 };
    bool        use_ai = false;
    bool        is_ai_thinking = false;
    
public:
    enum Status {
        BLACK_WIN,
        WHITE_WIN,
        TIED,
        NOT_END
    };
    
private:
    Status status = NOT_END;		//	当前棋局状态
    std::mt19937 mt;

public:
    
    Gomoku() : mt(std::random_device{}()) {
        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j < board_width; ++j)
                board[i][j] = NONE;
    }

    Status get_status() {
        return status;
    }

    bool drop(int i, int j, bool is_black) {
        if (is_black && is_ai_thinking)
            return false;
        if (is_black != current_is_black)
            return false;
        if (i < 0 || i >= board_height || j < 0 || j >= board_width)
            return false;
        if (board[i][j] != NONE)
            return false;
        if (status != NOT_END)
            return false;
        board[i][j] = current_is_black ? BLACK_PIECE : WHITE_PIECE;

        current_is_black = !current_is_black;
        last_drop = { j,i };
        return true;
    }

    void update() {
        if (status != NOT_END)
            return;

        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j <= board_width - 5; ++j)
                if (board[i][j] != NONE && equal(board[i][j], board[i][j + 1], board[i][j + 2], board[i][j + 3], board[i][j + 4])) {
                    if (board[i][j] == WHITE_PIECE)
                        status = WHITE_WIN;
                    else status = BLACK_WIN;
                }

        for (int i = 0; i < board_width; ++i)
            for (int j = 0; j <= board_height - 5; ++j)
                if (board[j][i] != NONE && equal(board[j][i], board[j + 1][i], board[j + 2][i], board[j + 3][i], board[j + 4][i])) {
                    if (board[j][i] == WHITE_PIECE)
                        status = WHITE_WIN;
                    else status = BLACK_WIN;
                }

        for (int i = 0; i <= board_height - 5; ++i)
            for (int j = 0; j <= board_width - 5; ++j)
                if (
                    (board[i][j] != NONE && equal(board[i][j], board[i + 1][j + 1], board[i + 2][j + 2], board[i + 3][j + 3], board[i + 4][j + 4]))
                    || (board[i + 4][j] != NONE && equal(board[i + 4][j], board[i + 3][j + 1], board[i + 2][j + 2], board[i + 1][j + 3], board[i][j + 4]))) {
                    if (board[i + 2][j + 2] == WHITE_PIECE)
                        status = WHITE_WIN;
                    else status = BLACK_WIN;
                }

        if (status == NOT_END && !current_is_black && use_ai)
            AIDrop();
    }

    void clear() {
        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j < board_width; ++j)
                board[i][j] = NONE;
        current_is_black = true;
        status = NOT_END;
        is_ai_thinking = false;
        last_drop = { -1,-1 };
    }

    void enable_ai(bool enable) {
        use_ai = enable;
    }

    Json::Value get_game_info() {
        Json::Value info;
        info["board"].resize(board_height);
        for (int i = 0; i < board_height; ++i) {
            info["board"][i].resize(board_width);
            for (int j = 0; j < board_width; ++j)
                info["board"][i][j] = std::string(1, board[i][j]);
        }
        info["last_drop"]["x"] = last_drop.x;
        info["last_drop"]["y"] = last_drop.y;
        info["current_is_black"] = current_is_black;
        return info;
    }

private:
    //	搜索算法部分
    int		max_search_depth = 1;   // 在服务器上运行，限制搜索深度为1
    POINT ai_drop_pos = { 0,0 };
    const char map[2]{ BLACK_PIECE, WHITE_PIECE };

    int situation(bool to_black) {
        int self = 0;
        int oppo = 0;
        const char map[2]{ BLACK_PIECE, WHITE_PIECE };
        const char chess_self = map[to_black];
        const char chess_oppo = map[!to_black];

        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j < board_width; ++j)
                if (board[i][j] == chess_self)
                    self += point_score(i, j, chess_self);
                else if (board[i][j] == chess_oppo)
                    oppo += point_score(i, j, chess_oppo);

        return self - oppo + (mt() & 7);
    }

    //	以该位置为中心2*2的区域内是否有其他棋子
    //	用此函数进行筛选能够大大地减少分支的数量
    bool is_nearby(int i, int j) {
        for (int di = -1; di < 2; ++di)
            for (int dj = -1; dj < 2; ++dj)
                if (i + di >= 0 && i + di < board_height && j + dj >= 0 && j + dj < board_width)
                    if (board[i + di][j + dj] != NONE)
                        return true;
        return false;
    }

    //	某一点的得分
    int point_score(int i, int j, char side) {
        int score = 1;

        //	*   *   *
        //	  * * *
        //  * * * * *
        //	  * * *
        //	*   *   *
        const POINT lines[4][9] = {
            {{-4,0},{-3,0},{-2,0},{-1,0},{0,0},{1,0},{2,0},{3,0},{4,0}},
            {{0,-4}, {0,-3}, {0,-2},{0,-1},{0,0},{0,1},{0,2},{0,3},{0,4}},
            {{-4,-4}, {-3,-3}, {-2,-2},{-1,-1},{0,0},{1,1},{2,2},{3,3},{4,4}},
            {{-4,4},{-3,3}, {-2,2},{-1,1},{0,0},{1,-1},{2,-2},{3,-3},{4,-4}}
        };

        //	五子棋术语
        //	参考：https://zhuanlan.zhihu.com/p/361201277
        static const std::string CHENG5 = "sssss";
        static const std::string HUO4 = " ssss ";
        static const std::string CHONG4[] = {
            " sssso",
            "s sss",
            "ss ss",
            "sss s",
            "ossss ",
        };
        static const std::string LIANHUO3[] = {
            " sss  ",
            "  sss ",
        };
        static const std::string TIAOHUO3[] = {
            " s ss ",
            " ss s ",
        };
        static const std::string MIAN3[] = {
            "  ssso",
            " s sso",
            " ss so",
            "osss  ",
            "oss s ",
            "os ss ",
            "ss  s",
            "s  ss",
            "s s s",
            "o sss o",
        };
        static const std::string HUO2[] = {
            "   ss ",
            "  ss  ",
            " ss   ",
            "  s s ",
            " s s  ",
        };
        static const std::string MIAN2[] = {
            "   sso",
            "  s so",
            " s  so",
            "s   s",
            "oss   ",
            "os s  ",
            "os  s ",

            "o  ss o",
            "o ss  o",
            "o s s o",
        };

        for (auto& line : lines) {
            std::string line_str;
            line_str.reserve(5);
            for (auto& point : line) {
                int x = j - point.x;
                int y = i - point.y;
                if (x < 0 || x >= board_width || y < 0 || y >= board_height)
                    line_str += 'n';
                else if (board[y][x] == side)
                    line_str += 's';
                else if (board[y][x] == NONE)
                    line_str += ' ';
                else line_str += 'o';
            }

            if (line_str.find(CHENG5) != std::string::npos)
                score += 5000000;
            if (line_str.find(HUO4) != std::string::npos)
                score += 100000;
            for (auto& chong4 : CHONG4)
                if (line_str.find(chong4) != std::string::npos) {
                    score += 16000;
                    break;
                }
            for (auto& lianhuo3 : LIANHUO3)
                if (line_str.find(lianhuo3) != std::string::npos) {
                    score += 8000;
                    break;
                }
            for (auto& tiaohuo3 : TIAOHUO3)
                if (line_str.find(tiaohuo3) != std::string::npos) {
                    score += 2000;
                    break;
                }
            for (auto& mian3 : MIAN3)
                if (line_str.find(mian3) != std::string::npos) {
                    score += 300;
                    break;
                }
            for (auto& huo2 : HUO2)
                if (line_str.find(huo2) != std::string::npos) {
                    score += 20;
                    break;
                }
            for (auto& mian2 : MIAN2)
                if (line_str.find(mian2) != std::string::npos) {
                    score += 2;
                    break;
                }
        }

        return score;
    }

    //	带alpha-beta剪枝的minimax搜索
    int search_max(int depth, bool to_black, int parent_beta) {
        int alpha = INT_MIN;
        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j < board_width; ++j) {
                if (board[i][j] == NONE && is_nearby(i, j)) {
                    board[i][j] = map[to_black];
                    int score;
                    if (depth < max_search_depth)
                        score = search_min(depth + 1, !to_black, alpha);
                    else score = situation(WHITE_PIECE);
                    board[i][j] = NONE;
                    if (score > alpha) {
                        alpha = score;
                        if (depth == 0)
                            ai_drop_pos = { j,i };
                        if (alpha >= parent_beta)
                            return alpha;
                    }
                }
            }
        return alpha;
    }

    int search_min(int depth, bool to_black, int parent_alpha) {
        int beta = INT_MAX;
        for (int i = 0; i < board_height; ++i)
            for (int j = 0; j < board_width; ++j) {
                if (board[i][j] == NONE && is_nearby(i, j)) {
                    board[i][j] = map[to_black];
                    int score;
                    if (depth < max_search_depth)
                        score = search_max(depth + 1, !to_black, beta);
                    else score = situation(WHITE_PIECE);
                    board[i][j] = NONE;
                    if (score < beta) {
                        beta = score;
                        if (beta <= parent_alpha)
                            return beta;
                    }
                }
            }
        return beta;
    }

    void AIDrop() {
        is_ai_thinking = true;
        int score = search_max(0, WHITE_PIECE, INT_MAX);
        drop(ai_drop_pos.y, ai_drop_pos.x, false);
        is_ai_thinking = false;
    }
};

#endif