//
// Created by qin on 10/26/23.
// Using : SVA2SMT input module output time
//
#include "SVA2SMT.h"
#include <assert.h>
#include <fstream>

std::vector<Token> tokens;
SmtInformation Smt;
ASTNode *RootASTNode;

extern ASTNode *build_ast();

// Tokenize函数
void read_parameter(int argv, char *argc[]);
std::string get_assert_property();
std::vector<Token> tokenize(const std::string& input);
void write_smt_lib2();
//// 解析方法定义

int main(int argv, char *argc[]) {
    read_parameter(argv, argc);
    tokens = tokenize(get_assert_property());
    RootASTNode = build_ast();
    write_smt_lib2();
    return 0;
}

void read_parameter(int argv,char *argc[]) {
    assert(argv == 6);
    Smt.InputFileName = argc[1];
    Smt.ModuleName = argc[2];
    Smt.OutputFileName = argc[3];
    Smt.Time = atoi(argc[4]);
    Smt.NeedFalse = atoi(argc[5]) == 0;
}

std::string get_assert_property() {
    std::fstream VerilogFile(Smt.InputFileName);
    std::string result = "";
    std::string code;
    std::string str("assert property");
    std::string expand("//");
    while(getline(VerilogFile, code)) {
        if (code.find(str) != std::string::npos && code.find(expand) == std::string::npos) {
            size_t head, tail;
            for (size_t i = 0; i < code.size(); i++) {
                if (code[i] == '(') {
                    head = i;
                    break;
                }
            }
            for (size_t i = code.size(); i != 0; i--) {
                if (code[i-1] == ')') {
                    tail = i;
                    break;
                }
            }
            result = std::string(code, head, tail - head);
            break;
        }
    }
    VerilogFile.close();
    assert(!result.empty() && "there must be a assert property in Verilog file!");
    std::cerr << result << std::endl;
    return result;
}

// Tokenize函数
std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;

    std::string str = input;

    while (!str.empty()) {
        std::smatch match;
        Token token;
        bool found = false;
        for (int i = 0; i < TOKEN_TYPE_NUM; i++) {
            if (std::regex_search(str.cbegin(), str.cend(), match, regexPatterns[i])) {
                Token token;
                if (!match.empty()) {
                    std::string fix_token = match[0];
                    if (str.find(fix_token) == 0) {
                        token.type = static_cast<TokenType>(i);
                        token.value = fix_token;
                        tokens.push_back(token);
                        str.erase(0, fix_token.length());
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) {
            str.erase(0, 1);
        }
    }
    return tokens;
}

void write_smt_lib2() {
    std::cout << RootASTNode->to_string() << std::endl;
    std::ofstream out(Smt.OutputFileName);
    for (size_t i = 1; i < Smt.Time; i+=2) {
        std::string assertName = "Assert_" + std::to_string(i);
        std::string smtExpr = RootASTNode->to_smt_lib2(i);
        out << "(declare-const " << assertName << " " << "Bool)" << std::endl;
        if (Smt.NeedFalse && !RootASTNode->has_overlap()) {
            out << "(assert (= " << assertName << " (not " << smtExpr << ")))" << std::endl;
        } else {
            out << "(assert (= " << assertName << " " << smtExpr << "))" << std::endl;
        }
    }
    out << "(assert (= true (or";
    for (size_t i = 1; i < Smt.Time; i+=2) {
        std::string assertName = "Assert_" + std::to_string(i);
        out << " " << assertName;
    }
    out << ")))" << std::endl;
    out.close();
}
