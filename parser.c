#include "compiler.h"
#include "helpers/vector.h"
#include <assert.h>

static struct compile_process *current_process;
static struct token *parser_last_token;
extern struct expressionable_op_precedence_group op_precedence[TOTAL_OPERATOR_GROUPS];
struct history
{
    int flags;
};

struct history* history_begin(int flags)
{
    struct history* history = calloc(1, sizeof(struct history));
    history->flags = flags;
    return history;
}

struct history* history_down(struct history* history, int flags)
{
    struct history* new_history = calloc(1, sizeof(struct history));
    memcpy(new_history, history, sizeof(struct history));
    new_history->flags = flags;
    return new_history;
}

int parse_expressionable_single(struct history* history);
void parse_expressionable(struct history* history);


static void parser_ignore_nl_or_comment(struct token *token)
{
    while (token && token_is_nl_or_comment_or_newline_seperator(token))
    {
        // skip the token
        vector_peek(current_process->token_vec);
        token = vector_peek_no_increment(current_process->token_vec);
    }
}
static struct token *token_next()
{
    struct token *next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);
    current_process->pos = next_token->pos;
    parser_last_token = next_token;
    return vector_peek(current_process->token_vec);
};

static struct token* token_peek_next()
{
    struct token* next_token = vector_peek_no_increment(current_process->token_vec);
    parser_ignore_nl_or_comment(next_token);  // this might change where the next token is, e.g. skip the nl and comment
    return vector_peek_no_increment(current_process->token_vec);
}

void parse_single_token_to_node()
{
    struct token* token = token_next();
    struct node* node = NULL;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        node = node_create(&(struct node){.type=NODE_TYPE_NUMBER, .llnum=token->llnum});
    break;
    
    case TOKEN_TYPE_IDENTIFIER:
        node = node_create(&(struct node){.type=NODE_TYPE_IDENTIFIER, .sval=token->sval});
    break;
    
    case TOKEN_TYPE_STRING:
        node = node_create(&(struct node){.type=NODE_TYPE_STRING, .sval=token->sval});
    break;

    default:
        compiler_error(current_process, "This is not a single token that can be converted to a node");
    }
}

void parse_expressionable_for_op(struct history* history, const char* op)
{
    parse_expressionable(history);
}

static int parser_get_precedence_for_operator(const char* op, struct expressionable_op_precedence_group** group_out)
{
    *group_out = NULL;
    for (int i = 0; i < TOTAL_OPERATOR_GROUPS; i++)
    {
        for (int b = 0; op_precedence[i].operators[b]; b++)  // iterate through all operators in the same group
        {
            const char* _op = op_precedence[i].operators[b];
            if (S_EQ(op, _op))
            {
                *group_out = &op_precedence[i];
                return i;  // this is the precedence
            }
        }
    }

    return -1; // cannot find the operator
}

static bool parser_left_op_has_priority(const char* op_left, const char* op_right)
{
    struct expressionable_op_precedence_group* group_left = NULL;
    struct expressionable_op_precedence_group* group_right = NULL;

    if (S_EQ(op_left, op_right))
    {
        return false;
    }

    int precedence_left = parser_get_precedence_for_operator(op_left, &group_left);
    int precedence_right = parser_get_precedence_for_operator(op_right, &group_right);
    if (group_left->associtivity == ASSOCIATIVITY_RIGHT_TO_LEFT)
    {
        return false;
    }

    return precedence_left <= precedence_right;
}

void parser_node_shift_children_left(struct node* node)
{
    /**         make_exp_node
     *     A         A        C
     *    / \       / \      / \
     *   B  C   => B  D  => A  E 
     *     / \             / \
     *    D  E            B  D
     * 
     *    *                  +
     *   / \                / \
     * 50  +       =>      *  120
     *    / \             / \
     *  20 120           50 20 
     */  
    assert(node->type == NODE_TYPE_EXPRESSION);
    assert(node->exp.right->type == NODE_TYPE_EXPRESSION);

    const char* right_op = node->exp.right->exp.op;
    struct node* new_exp_left_node = node->exp.left;
    struct node* new_exp_right_node = node->exp.right->exp.left;
    make_exp_node(new_exp_left_node, new_exp_right_node, node->exp.op);

    struct node* new_left_operand = node_pop();
    struct node* new_right_operand = node->exp.right->exp.right;
    node->exp.left = new_left_operand;
    node->exp.right = new_right_operand;
    node->exp.op = right_op;

}

void parser_reorder_expression(struct node** node_out)
{
    struct node* node = *node_out;
    if (node->type != NODE_TYPE_EXPRESSION)
    {
        return;
    }

    // No expressions, nothing to do
    if (node->exp.left->type != NODE_TYPE_EXPRESSION &&
        node->exp.right && node->exp.right->type != NODE_TYPE_EXPRESSION)
    {
        return;
    }

    // 50 + E(50*20), left node is 50, right node is 50 * 20 which contains an expression
    if (node->exp.left->type != NODE_TYPE_EXPRESSION &&
        node->exp.right && node->exp.right->type == NODE_TYPE_EXPRESSION)
    {
        const char* right_op = node->exp.right->exp.op;
        if (parser_left_op_has_priority(node->exp.op, right_op))
        {
            // 50*E(20+120)
            // E(50*20)+120
            parser_node_shift_children_left(node);

            parser_reorder_expression(&node->exp.left);
            parser_reorder_expression(&node->exp.right);
        }
    }

}


void parse_exp_normal(struct history* history)
{
    struct token* op_token = token_peek_next();
    char* op = op_token->sval;
    struct node* node_left = node_peek_expressionable_or_null();

    if (!node_left)
    {
        return;
    }

    // Pop off the operator token
    token_next();

    // Pop off the left node
    node_pop();
    node_left->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    parse_expressionable_for_op(history_down(history, history->flags), op);
    
    struct node* node_right = node_pop();
    node_right->flags |= NODE_FLAG_INSIDE_EXPRESSION;
    
    make_exp_node(node_left, node_right, op);
    struct node* exp_node = node_pop();

    // Reorder the expression
    parser_reorder_expression(&exp_node);
    node_push(exp_node);
}

int parse_exp(struct history* history)
{
    parse_exp_normal(history);
    return 0;
}

int parse_expressionable_single(struct history* history)
{
    struct token* token = token_peek_next();
    if (!token)
    {
        return -1;
    }

    history->flags |= NODE_FLAG_INSIDE_EXPRESSION;  // compound assignment, bitwise OR
    int res = -1;

    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
        parse_single_token_to_node();
        res = 0;
    break;

    case TOKEN_TYPE_OPERATOR:
        parse_exp(history);
        res = 0;
    break;
    
    default:
        break;
    }
}

void parse_expressionable(struct history* history)
{
    while (parse_expressionable_single(history) == 0)
    {

    }
}

int parse_next()
{
    struct token* token = token_peek_next();
    if (!token)
    {
        return -1;
    }

    int res = 0;
    switch (token->type)
    {
    case TOKEN_TYPE_NUMBER:
    case TOKEN_TYPE_IDENTIFIER:
    case TOKEN_TYPE_STRING:
        parse_expressionable(history_begin(0));
        // parse_single_token_to_node();
        break;
    
    default:
        break;
    }
    return 0;
}

int parse(struct compile_process *process)
{
    current_process = process;
    parser_last_token = NULL;

    node_set_vector(process->node_vec, process->node_tree_vec);

    struct node *node = NULL;

    vector_set_peek_pointer(process->token_vec, 0);
    while (parse_next() == 0)
    {
        node = node_peek();
        vector_push(process->node_tree_vec, &node);
    }
    return PARSE_ALL_OK;
}