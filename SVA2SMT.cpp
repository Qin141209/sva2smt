#include "SVA2SMT.h"
#include <assert.h>
#include <stack>

extern std::vector<Token> tokens;
extern SmtInformation Smt;

static std::stack<Token> TokenStack;
static std::stack<ASTNode *> AstStack;
static size_t VecIndex;

ASTNode *build_ast();
static void act_stack_top();

static void build_var_node();
static void build_var_select_node();
static void build_select_value_node();

static void build_data_node();
static void build_zx_data_node();
static void build_paren_node();
static void build_logic_node();
static void build_compare_node();
static void build_delay_node();
static void build_overlap_node();

// 解析器实现
ASTNode *build_ast() {
    for (VecIndex = 0; VecIndex < tokens.size(); VecIndex++) {
        TokenStack.push(tokens[VecIndex]);
        if (tokens[VecIndex].type == TokenType::RPAREN
        || tokens[VecIndex].type == TokenType::RBRACKET
        || tokens[VecIndex].type == TokenType::IDENTIFIER
        || tokens[VecIndex].type == TokenType::XZ_VALUE) {
            act_stack_top();
        }
    }
    assert(AstStack.size() == 1);
    return AstStack.top();
}

static void act_stack_top() {
    if (!TokenStack.empty()) {
        switch(TokenStack.top().type) {
            case TokenType::RPAREN:
                build_paren_node();
                break;
            case TokenType::RBRACKET:
                build_var_select_node();
                break;
            case TokenType::DELAY_CONTROL:
                build_delay_node();
                break;
            case TokenType::AND:
            case TokenType::OR:
                build_logic_node();
                break;
            case TokenType::EQUALS:
            case TokenType::NOT_EQUALS:
            case TokenType::LESS_EQUALS:
            case TokenType::GREATER_EQUALS:
            case TokenType::LESS:
            case TokenType::GREATER:
                build_compare_node();
                break;
            case TokenType::IDENTIFIER:
                build_var_node();
                break;
            case TokenType::BIT_SELECT:
                build_select_value_node();
                break;
            case TokenType::DATA_B:
                build_data_node();
                break;
            case TokenType::XZ_VALUE:
                build_zx_data_node();
                break;
            case TokenType::OVERLAP:
            case TokenType::NOT_OVERLAP:
                build_overlap_node();
                break;
            default:
                assert(false && "wrong case item check stack top here!");
                break;
        }
    }
}

static void build_var_node() {
    Token &top = TokenStack.top();
    ASTNode *node = new Identifier(top.value);
    AstStack.push(node);
    TokenStack.pop();
}

static void build_var_select_node() {
    bool range = false;
    TokenStack.pop();
    while (TokenStack.top().type != TokenType::LBRACKET) {
        if (TokenStack.top().type == TokenType::BIT_SELECT) {
            act_stack_top();
        } else if (TokenStack.top().type == TokenType::COLON) {
            range = true;
            TokenStack.pop();
        } else {
            assert(false && "wrong case item check bit range here!");
        }
    }

    if (range) {
        BitValue *left = dynamic_cast<BitValue *>(AstStack.top());
        AstStack.pop();
        BitValue *right = dynamic_cast<BitValue *>(AstStack.top());
        AstStack.pop();
        Identifier *var = dynamic_cast<Identifier *>(AstStack.top());
        AstStack.pop();
        ASTNode *node = new RangeSelect(var, left, right);
        AstStack.push(node);
    } else {
        BitValue *selector = dynamic_cast<BitValue *>(AstStack.top());
        AstStack.pop();
        Identifier *var = dynamic_cast<Identifier *>(AstStack.top());
        AstStack.pop();
        ASTNode *node = new BitSelect(var, selector);
        AstStack.push(node);
    }
    TokenStack.pop();
}

static void build_select_value_node() {
    Token &top = TokenStack.top();
    ASTNode *node = new BitValue(atoi(top.value.c_str()));
    AstStack.push(node);
    TokenStack.pop();
}

static void build_data_node() {
    Token &top = TokenStack.top();
    std::smatch match;
    std::regex str("[0-9]+");
    std::regex_search(top.value.cbegin(), top.value.cend(), match, str);
    std::string res = std::string(match[0]);
    unsigned width = atoi(res.c_str());
    std::string value = std::string(top.value, res.length() + 2, top.value.length());
    ASTNode *node = new DataValue(width, value);
    AstStack.push(node);
    TokenStack.pop();
}

static void build_zx_data_node() {
    Token &top = TokenStack.top();
    std::smatch match;
    std::regex str("[0-9]+");
    std::regex_search(top.value.cbegin(), top.value.cend(), match, str);
    std::string res = std::string(match[0]);
    unsigned width = atoi(res.c_str());
    std::string value = std::string(top.value.length() - res.length() - 2, '0');
    ASTNode *node = new DataValue(width, value);
    AstStack.push(node);
    TokenStack.pop();
}

static void build_paren_node() {
    TokenStack.pop();
    while (TokenStack.top().type != TokenType::LPAREN) {
        act_stack_top();
    }
    assert(!AstStack.empty());
    ASTNode *node = new ParenExpression(AstStack.top());
    AstStack.pop();
    AstStack.push(node);
    TokenStack.pop();
}

static void build_logic_node() {
    assert(AstStack.size() >= 2);
    ASTNode *right = AstStack.top();
    AstStack.pop();
    ASTNode *left = AstStack.top();
    AstStack.pop();
    BinaryOperator op = TokenStack.top().type == TokenType::AND ? BinaryOperator::OP_AND : BinaryOperator::OP_OR;
    ASTNode *node = new LogicAndOrOperation(op, left, right);
    AstStack.push(node);
    TokenStack.pop();
}

static void build_compare_node() {
    assert(AstStack.size() >= 2);
    ASTNode *right = AstStack.top();
    AstStack.pop();
    ASTNode *left = AstStack.top();
    AstStack.pop();

    CompareExpression::CompareType type;

    switch (TokenStack.top().type)
    {
    case TokenType::EQUALS:
        type = CompareExpression::CompareType::EQUAL;
        break;
    case TokenType::NOT_EQUALS:
        type = CompareExpression::CompareType::NEQUAL;
        break;
    case TokenType::LESS_EQUALS:
        type = CompareExpression::CompareType::LEQUAL;
        break;
    case TokenType::GREATER_EQUALS:
        type = CompareExpression::CompareType::GEQUAL;
        break;
    case TokenType::LESS:
        type = CompareExpression::CompareType::LESS;
        break;
    case TokenType::GREATER:
        type = CompareExpression::CompareType::GREATER;
        break;
        default:
        assert(false && "unsupported compare expression!");
        break;
    }

    ASTNode *node = new CompareExpression(left, right, type);

    AstStack.push(node);
    TokenStack.pop();
}

static void build_delay_node() {
    assert(AstStack.size() >= 2);
    ASTNode *right = AstStack.top();
    AstStack.pop();
    ASTNode *left = AstStack.top();
    AstStack.pop();
    std::string value = std::string(TokenStack.top().value, 2, TokenStack.top().value.length());
    unsigned delay = atoi(value.c_str());
    ASTNode *node = new DelayControl(delay, left, right);
    AstStack.push(node);
    TokenStack.pop();
}

static void build_overlap_node() {
    assert(AstStack.size() >= 2);
    ASTNode *right = AstStack.top();
    AstStack.pop();
    ASTNode *left = AstStack.top();
    AstStack.pop();
    bool delay = TokenStack.top().type == NOT_OVERLAP;
    ASTNode *node = new OverlapExpression(left, right, delay);
    AstStack.push(node);
    TokenStack.pop();
}

std::string Identifier::to_smt_lib2(unsigned time) const {
    return GET_DECLARE_NAME(Smt, name, time);
}

std::string BitValue::to_smt_lib2(unsigned time) const {
    return std::to_string(value);
}

std::string DataValue::to_smt_lib2(unsigned time) const {
    std::string res = "#b" + data;
    return res;
}

std::string BitSelect::to_smt_lib2(unsigned time) const {
    std::string bit = selector->to_smt_lib2(time);
    std::string var = variable->to_smt_lib2(time);
    std::string res = "(_ extract " + bit + " " + bit + " (" + var + "))";
    return res;
}

std::string RangeSelect::to_smt_lib2(unsigned time) const {
    std::string left = left_selector->to_smt_lib2(time);
    std::string right = right_selector->to_smt_lib2(time);
    std::string var = variable->to_smt_lib2(time);
    std::string res = "(_ extract " + left + " " + right + " (" + var + "))";
    return res;
}

std::string LogicAndOrOperation::to_smt_lib2(unsigned time) const {
    std::string l_smt = left->to_smt_lib2(time);
    std::string r_smt = right->to_smt_lib2(time);
    std::string bv_op = op == BinaryOperator::OP_AND ? "and" : "or";
    std::string res = "(" + bv_op + " " + l_smt + " " + r_smt + ")";
    return res;
}

std::string DelayControl::to_smt_lib2(unsigned time) const {
    std::string l_smt = left->to_smt_lib2(time);
    std::string r_smt = right->to_smt_lib2(time + delay * TIME_CLOCK);
    std::string res = "(and " + l_smt + " " + r_smt + ")";
    return res;
}

std::string ParenExpression::to_smt_lib2(unsigned time) const {
    return expression->to_smt_lib2(time);
}

std::string OverlapExpression::to_smt_lib2(unsigned time) const {
    std::string l_smt = left->to_smt_lib2(time);
    std::string r_smt = right->to_smt_lib2(delay ? time + TIME_CLOCK : time);
    std::string res = "(and " + l_smt + " " + r_smt + ")";
    return res;
}

std::string CompareExpression::to_smt_lib2(unsigned time) const {
    std::string l_smt = left->to_smt_lib2(time);
    std::string r_smt = right->to_smt_lib2(time);
    std::string bv_op;

    switch (type)
        {
        case CompareExpression::CompareType::EQUAL:
            bv_op = "=";
            break;
        case CompareExpression::CompareType::NEQUAL:
            bv_op = "distinct";
            break;
        case CompareExpression::CompareType::LEQUAL:
            bv_op = "bvule";
            break;
        case CompareExpression::CompareType::GEQUAL:
            bv_op = "bvuge";
            break;
        case CompareExpression::CompareType::LESS:
            bv_op = "bvult";
            break;
        case CompareExpression::CompareType::GREATER:
            bv_op = "bvugt";
            break;
        default:
            assert(false && "unsupported compare expression!");
            break;
        }

    std::string res = "(" + bv_op + " " + l_smt + " " + r_smt + ")";
    return res;
}
