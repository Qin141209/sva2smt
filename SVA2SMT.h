#include <iostream>
#include <vector>
#include <string>
#include <regex>

#define TOKEN_TYPE_NUM 20
#define TIME_CLOCK 2
#define GET_DECLARE_NAME(generator, var, time) ("testbench." + generator.ModuleName + "_instance." + var + "_" + std::to_string(time) + "_1")

// Token类型
enum TokenType {
    IDENTIFIER,
    DATA_B,  // 4'b0000
    XZ_VALUE, // 4'bxxxx
    BIT_SELECT,
    AND,
    OR,
    DELAY_CONTROL,
    EQUALS,  // ==
    NOT_EQUALS, // !=
    LESS_EQUALS, // <=
    GREATER_EQUALS, // >=
    LESS,     // <
    GREATER,  // >
    OVERLAP,  // |->
    NOT_OVERLAP, // |=>
    LPAREN,   // (
    RPAREN,   // )
    COLON,    // :
    LBRACKET, // [
    RBRACKET, // ]
    OTHER
};

// Token结构
struct Token {
    TokenType type;
    std::string value;
};

// 二进制操作符枚举
enum BinaryOperator {
    OP_AND,
    OP_OR,
};

// 正则表达式模式
const std::regex regexPatterns[] = {
    std::regex("[a-zA-Z_][a-zA-Z_0-9]*"), // IDENTIFIER
    std::regex("\\d+'[bB][01]+"), // DATA_B
    std::regex("\\d+'[bB][xz]+"),  // XZ_VALUE
    std::regex("[0-9]+"), // BIT_SELECT
    std::regex("\\&\\&"), // LOGIC_AND
    std::regex("\\|\\|"), // LOGIC_OR
    std::regex("##\\d+"), // DELAY_CONTROL
    std::regex("=="), // EQUALS
    std::regex("!="), // NOT_EQUALS
    std::regex("\\<="), // LESS_EQUAL
    std::regex("\\>="), // GREATER_EQUAL
    std::regex("\\<"),  // LESS
    std::regex("\\>"),  // GREATER
    std::regex("\\|->"), // OVERLAP
    std::regex("\\|=>"), // NOT_OVERLAP
    std::regex("\\("), // LPAREN
    std::regex("\\)"), // RPAREN
    std::regex("\\:"), // COLON
    std::regex("\\["), // LBRACKET
    std::regex("\\]")  // RBRACKET
};

struct SmtInformation {
    std::string InputFileName;
    std::string OutputFileName;
    std::string ModuleName;
    unsigned Time;
    bool NeedFalse;
};

class ASTNode {
public:
    virtual ~ASTNode() {}  // Ensure a virtual destructor for proper cleanup.

    virtual std::string to_string() const = 0;
    // Other common methods and properties shared among all AST nodes can be defined here.
    virtual std::string to_smt_lib2(unsigned time) const = 0;

    virtual bool has_overlap() const = 0;
};

// 标识符节点
class Identifier : public ASTNode {
public:
    Identifier(const std::string& name) : name(name) {}

    std::string to_string() const override {
        return name;
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return false;
    }

private:
    std::string name;
};

class BitValue : public ASTNode {
public:
    BitValue(const int& value) : value(value) {}

    std::string to_string() const override {
        return std::to_string(value);
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return false;
    }

private:
    int value;
};

class DataValue : public ASTNode {
public:
    DataValue(const unsigned& width, const std::string& data) : width(width), data(data) {}

    std::string to_string() const override {
        return std::to_string(width) + "'b" + data;
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return false;
    }

private:
    unsigned width;
    std::string data;
};


// 变量位选节点
class BitSelect : public ASTNode {
public:
    BitSelect(Identifier* variable, BitValue* selector)
            : variable(variable), selector(selector) {}

    std::string to_string() const override {
        return variable->to_string() + "[" + selector->to_string() + "]";
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return false;
    }

private:
    Identifier* variable;
    BitValue* selector;
};

// RangeSelect节点
class RangeSelect : public ASTNode {
public:
    RangeSelect(Identifier* variable, BitValue* left_selector, BitValue* right_selector)
        : variable(variable), left_selector(left_selector), right_selector(right_selector) {}

    std::string to_string() const override {
        return variable->to_string() + "[" + left_selector->to_string() + ":" + right_selector->to_string() + "]";
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return false;
    }

private:
    Identifier* variable;
    BitValue* left_selector;
    BitValue* right_selector;
};

// 与/或运算节点
class LogicAndOrOperation : public ASTNode {
public:
    LogicAndOrOperation(BinaryOperator op, ASTNode* left, ASTNode* right)
            : op(op), left(left), right(right) {}

    std::string to_string() const override {
        const std::string opStr = (op == OP_AND) ? "&&" : "||";
        return left->to_string() + " " + opStr + " " + right->to_string();
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return (left->has_overlap() | right->has_overlap());
    }

private:
    BinaryOperator op;
    ASTNode* left;
    ASTNode* right;
};

// 延时控制节点
class DelayControl : public ASTNode {
public:
    DelayControl(const unsigned& delay, ASTNode* left, ASTNode* right) : delay(delay), left(left), right(right) {}

    std::string to_string() const override {
        return left->to_string() + " ##" + std::to_string(delay) + " " + right->to_string();
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return true;
    }

private:
    unsigned delay;
    ASTNode* left;
    ASTNode* right;
};

// 括号表达式节点
class ParenExpression : public ASTNode {
public:
    ParenExpression(ASTNode* expression) : expression(expression) {}

    std::string to_string() const override {
        return "(" + expression->to_string() + ")";
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return expression->has_overlap();
    }

private:
    ASTNode* expression;
};

class OverlapExpression : public ASTNode {
public:
    OverlapExpression(ASTNode *left, ASTNode *right, bool delay) : left(left), right(right), delay(delay) {}

    std::string to_string() const override {
        return (left->to_string() + (delay ? "|=>" : "|->") + right->to_string());
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return true;
    }

private:
    ASTNode *left;
    ASTNode *right;
    bool delay;
};

class CompareExpression : public ASTNode {
public:
    enum CompareType{EQUAL, NEQUAL, LEQUAL, GEQUAL, LESS, GREATER};
    CompareExpression(ASTNode *left, ASTNode *right, CompareType type) : left(left), right(right), type(type) {}

    std::string get_type() const {
        switch (type)
        {
        case EQUAL:
            return "==";
        case NEQUAL:
            return "!=";
        case LEQUAL:
            return "<=";
        case GEQUAL:
            return ">=";
        case LESS:
            return "<";
        case GREATER:
            return ">";
        default:
            break;
        }
        return "";
    }

    std::string to_string() const override {
        return left->to_string() + get_type() + right->to_string();
    }

    std::string to_smt_lib2(unsigned time) const override;

    bool has_overlap() const override {
        return (left->has_overlap() | right->has_overlap());
    }

private:
    ASTNode *left;
    ASTNode *right;
    CompareType type;
};








