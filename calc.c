// Name: Nihad Naghizade, Student ID: 251ADB080
// Repository: https://github.com/nihadnagizada-arch/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: OpenAI ChatGPT (AI assistance for understanding parser design,
// error-position rules, and testing).
// AI conversation history: https://chatgpt.com/share/6ade3a33-7ca4-83ed-b3ce-0723565f5e2c
// I used AI tools to support my learning, improve my understanding
// of C programming, and explore parser design and testing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

typedef enum {
    TOKEN_END,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_POWER,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_INVALID
} TokenType;

typedef struct {
    TokenType type;
    long long integer_value;
    double float_value;
    size_t position;
} Token;

typedef struct {
    const char *text;
    size_t length;
    size_t index;
    size_t base_position;
    Token current;
    size_t error_position;
    int has_error;
} Parser;

typedef struct {
    int is_float;
    long long integer;
    double floating;
} Value;
// Prompt 1: Help me build an arithmetic parser in C step by step,
// giving me partial code and letting me implement the functions.
// Prompt 2: Help me implement tokenization and track the correct
// character positions for parsing errors.
// I made the tokenizer read one token at a time and keep its position,
// so I can report the exact location of a parsing error.
// I changed the number buffer to use dynamic memory instead of
// a fixed-size array, allowing longer numeric tokens to be read.
static void next_token(Parser *p)
{
    while (p->index < p->length &&
           (p->text[p->index] == ' ' ||
            p->text[p->index] == '\t' ||
            p->text[p->index] == '\r')) {
        p->index++;
    }

    p->current.position = p->base_position + p->index;
    p->current.integer_value = 0;
    p->current.float_value = 0.0;

    if (p->index >= p->length || p->text[p->index] == '\n') {
        p->current.type = TOKEN_END;
        return;
    }

    char c = p->text[p->index];

    if (isdigit((unsigned char)c)) {
        size_t start = p->index;

        while (p->index < p->length &&
               isdigit((unsigned char)p->text[p->index])) {
            p->index++;
        }

        int is_float = 0;

        if (p->index < p->length && p->text[p->index] == '.') {
            is_float = 1;
            p->index++;

            if (p->index >= p->length ||
                !isdigit((unsigned char)p->text[p->index])) {
                p->current.type = TOKEN_INVALID;
                p->current.position = p->base_position + p->index - 1;
                return;
            }

            while (p->index < p->length &&
                   isdigit((unsigned char)p->text[p->index])) {
                p->index++;
            }
        }
        size_t len = p->index - start;
        char *buffer = malloc(len + 1);

        if (buffer == NULL) {
            // Allocation failure must not be mistaken for a valid token. 
            p->current.type = TOKEN_INVALID;
            p->current.position = p->base_position + start;
            return;
        }

        memcpy(buffer, p->text + start, len);
        buffer[len] = '\0';

        char *end;
        errno = 0;

        if (is_float) {
            double value = strtod(buffer, &end);

            if (errno == ERANGE || *end != '\0' || !isfinite(value)) {
                p->current.type = TOKEN_INVALID;
                p->current.position = p->base_position + start;
            } else {
                p->current.type = TOKEN_FLOAT;
                p->current.float_value = value;
            }
        } else {
            long long value = strtoll(buffer, &end, 10);

            if (errno == ERANGE || *end != '\0') {
                p->current.type = TOKEN_INVALID;
                p->current.position = p->base_position + start;
            } else {
                p->current.type = TOKEN_INTEGER;
                p->current.integer_value = value;
            }
        }

        free(buffer);
        return;
    }

    p->index++;

    switch (c) {
        case '+':
            p->current.type = TOKEN_PLUS;
            break;
        case '-':
            p->current.type = TOKEN_MINUS;
            break;
        case '*':
            if (p->index < p->length && p->text[p->index] == '*') {
                p->index++;
                p->current.type = TOKEN_POWER;
            } else {
                p->current.type = TOKEN_MULTIPLY;
            }
            break;
        case '/':
            p->current.type = TOKEN_DIVIDE;
            break;
        case '(':
            p->current.type = TOKEN_LEFT_PAREN;
            break;
        case ')':
            p->current.type = TOKEN_RIGHT_PAREN;
            break;
        default:
            p->current.type = TOKEN_INVALID;
            break;
    }
}

static Value parse_expr(Parser *p);

static Value make_integer(long long n)
{
    Value v = {0};
    v.integer = n;
    return v;
}

static Value make_float(double n)
{
    Value v = {0};
    v.is_float = 1;
    v.floating = n;
    return v;
}

static Value primary(Parser *p)
{
    if (p->current.type == TOKEN_INTEGER) {
        long long n = p->current.integer_value;
        next_token(p);
        return make_integer(n);
    }

    if (p->current.type == TOKEN_FLOAT) {
        double n = p->current.float_value;
        next_token(p);
        return make_float(n);
    }
    
// I handle parentheses by parsing the inner expression first.
// I use the current token position to report where a closing
// parenthesis was expected but not found.
    if (p->current.type == TOKEN_LEFT_PAREN) {
        next_token(p);
        Value result = parse_expr(p);

        if (p->has_error) {
            return result;
        }

        if (p->current.type != TOKEN_RIGHT_PAREN) {
            p->has_error = 1;
            p->error_position = p->current.position;
            return result;
        }

        next_token(p);
        return result;
    }

    p->has_error = 1;
    p->error_position = p->current.position;
    return make_integer(0);
}

// I made power() call itself for the exponent so that powers
// are evaluated from right to left, as in 2**(3**2).
// Prompt 3: Explain how to implement operator precedence,
// parentheses, and right-associative exponentiation.
static Value power(Parser *p)
{
    Value left = primary(p);

    if (p->has_error) {
        return left;
    }

    if (p->current.type != TOKEN_POWER) {
        return left;
    }

    size_t operator_position = p->current.position;

    next_token(p);

    size_t exponent_position = p->current.position;
    Value right = power(p);

    if (p->has_error) {
        return left;
    } 
    if (!left.is_float && !right.is_float) {
        // I reject negative integer exponents and report the start
        // of the exponent, as required by the input rules.
        if (right.integer < 0) {
            p->has_error = 1;
            p->error_position = exponent_position;
            return make_integer(0);
        }

        long long result = 1;

        for (long long i = 0; i < right.integer; i++) {
            result *= left.integer;
        }

        return make_integer(result);
    }

    double a = left.is_float
        ? left.floating : (double)left.integer;

    double b = right.is_float
        ? right.floating : (double)right.integer;

    double result = pow(a, b);
        // I check the floating-point result because overflow or an
    // undefined result must be reported at the operator position.
    if (!isfinite(result)) {
        p->has_error = 1;
        p->error_position = operator_position;
        return make_float(0.0);
    }

    return make_float(result);
}

static Value term(Parser *p)
{
    Value left = power(p);

    while (!p->has_error &&
           (p->current.type == TOKEN_MULTIPLY ||
            p->current.type == TOKEN_DIVIDE)) {

        TokenType op = p->current.type;
        size_t operator_position = p->current.position;

        next_token(p);
        size_t right_position = p->current.position;

        Value right = power(p);

        if (p->has_error) {
            return left;
        }
// Prompt 4: Review my arithmetic operations, including integer division,
// floating-point calculations, division by zero, and negative exponents.
// I separated integer and floating-point calculations.
// This keeps integer division truncated toward zero, while
// expressions with a float are evaluated using double.

        if (!left.is_float && !right.is_float) {
            if (op == TOKEN_MULTIPLY) {
                left = make_integer(left.integer * right.integer);
            } else {
                // I report division by zero at the start of the
                // right operand rather than at the division operator.
                if (right.integer == 0) {
                    p->has_error = 1;
                    p->error_position = right_position;
                    return make_integer(0);
                }

                left = make_integer(left.integer / right.integer);
            }
        } else {
            double a = left.is_float
                ? left.floating : (double)left.integer;

            double b = right.is_float
                ? right.floating : (double)right.integer;

            if (op == TOKEN_DIVIDE && b == 0.0) {
                p->has_error = 1;
                p->error_position = right_position;
                return make_float(0.0);
            }

            double result;

            if (op == TOKEN_MULTIPLY) {
                result = a * b;
            } else {
                result = a / b;
            }

            if (!isfinite(result)) {
                p->has_error = 1;
                p->error_position = operator_position;
                return make_float(0.0);
            }

            left = make_float(result);
        }
    }

    return left;
}

// I handle addition and subtraction after term() has evaluated
// multiplication, division, and power expressions. This keeps
// the operators in the precedence order required by the task.
static Value parse_expr(Parser *p)
{
    Value left = term(p);

    while (!p->has_error &&
           (p->current.type == TOKEN_PLUS ||
            p->current.type == TOKEN_MINUS)) {

        TokenType op = p->current.type;
        size_t operator_position = p->current.position;

        next_token(p);
        Value right = term(p);

        if (p->has_error) {
            return left;
        }

        if (!left.is_float && !right.is_float) {
            if (op == TOKEN_PLUS) {
                left = make_integer(left.integer + right.integer);
            } else {
                left = make_integer(left.integer - right.integer);
            }
        } else {
            double a = left.is_float
                ? left.floating : (double)left.integer;

            double b = right.is_float
                ? right.floating : (double)right.integer;

            double result;

            if (op == TOKEN_PLUS) {
                result = a + b;
            } else {
                result = a - b;
            }

            if (!isfinite(result)) {
                p->has_error = 1;
                p->error_position = operator_position;
                return make_float(0.0);
            }

            left = make_float(result);
        }
    }

    return left;
}

static Value evaluate_line(const char *line, size_t length,
                           size_t line_start, size_t *error_position,
                           int *has_error)
{
    Parser p = {0};

    p.text = line;
    p.length = length;
    p.base_position = line_start;
    
    // I pass the line's starting position to the parser so error
    // positions count from the beginning of the whole input file,
        // including earlier lines and comment lines.

    next_token(&p);
    Value result = parse_expr(&p);

    if (!p.has_error && p.current.type != TOKEN_END) {
        p.has_error = 1;
        p.error_position = p.current.position;
    }

    *has_error = p.has_error;

    if (p.has_error) {
        *error_position = p.error_position;
    }

    return result;
}
static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static char *input_base_name(const char *path)
{
    const char *base = base_name(path);
    size_t length = strlen(base);

    if (length >= 4 && strcmp(base + length - 4, ".txt") == 0) {
        length -= 4;
    }

    char *result = malloc(length + 1);
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, base, length);
    result[length] = '\0';
    return result;
}

static char *join_path(const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);
    int needs_slash = directory_length > 0 &&
                      directory[directory_length - 1] != '/';

    size_t max_size = (size_t)-1;

    if (name_length > max_size - (size_t)needs_slash - 1 ||
    directory_length > max_size - name_length -
                       (size_t)needs_slash - 1) {
    return NULL;
}

    size_t total = directory_length + name_length +
                   (size_t)needs_slash + 1;
    char *result = malloc(total);

    if (result == NULL) {
        return NULL;
    }

    snprintf(result, total, "%s%s%s", directory,
             needs_slash ? "/" : "", name);
    return result;
}
static int read_line(FILE *file, char **line, size_t *capacity,
                     size_t *length)
{
    // Prompt 5: Help me review file handling, dynamic line reading,
    // output paths, memory allocation, and overflow checks.
    // I grow the line buffer only when it is full, so long lines
    // can be read without a fixed maximum line length.
    *length = 0;
    int ch;

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            break;
        }

        if (*length + 1 >= *capacity) {
            size_t new_capacity =
                (*capacity == 0) ? 256 : *capacity * 2;

            if (new_capacity <= *capacity) {
                return -1;
            }

            char *new_line = realloc(*line, new_capacity);
            if (new_line == NULL) {
                return -1;
            }

            *line = new_line;
            *capacity = new_capacity;
        }

        (*line)[(*length)++] = (char)ch;
    }

    if (ch == EOF && *length == 0) {
        return ferror(file) ? -1 : 0;
    }

    (*line)[*length] = '\0';
    return 1;
}
static int process_file(const char *input_path, const char *output_path)
{
    FILE *input = fopen(input_path, "r");
    if (input == NULL) {
        fprintf(stderr, "Cannot open input file: %s\n", input_path);
        return 1;
    }
    
        // I write results to a temporary file first so that if any
        // expression fails, I can output only ERROR:<pos> instead
        // of leaving partial results in the final output file.
 
    FILE *temporary = tmpfile();
    if (temporary == NULL) {
        fprintf(stderr, "Cannot create temporary file.\n");
        fclose(input);
        return 1;
    }

    char *line = NULL;
    size_t capacity = 0;
    size_t length = 0;
    size_t error_position = 0;
    int has_error = 0;
    int read_status;

    while (1) {
        long offset = ftell(input);
        if (offset < 0) {
            fprintf(stderr, "Cannot determine input position.\n");
            free(line);
            fclose(input);
            fclose(temporary);
            return 1;
        }

        read_status = read_line(input, &line, &capacity, &length);

        if (read_status == 0) {
            break;
        }

        if (read_status < 0) {
            fprintf(stderr, "Error reading input file: %s\n", input_path);
            free(line);
            fclose(input);
            fclose(temporary);
            return 1;
        }

        // The offset is zero-based; expression positions are one-based.
        size_t line_start = (size_t)offset + 1;

        size_t first = 0;
        while (first < length &&
               isspace((unsigned char)line[first])) {
            first++;
        }

        
        if (first == length || line[first] == '#') {
            continue;
        }

        Value result = evaluate_line(
            line, length, line_start, &error_position, &has_error);

        if (has_error) {
            break;
        }

        if (result.is_float) {
            if (fprintf(temporary, "%.15g\n", result.floating) < 0) {
                fprintf(stderr, "Error writing temporary results.\n");
                free(line);
                fclose(input);
                fclose(temporary);
                return 1;
            }
        } else {
            if (fprintf(temporary, "%lld\n", result.integer) < 0) {
                fprintf(stderr, "Error writing temporary results.\n");
                free(line);
                fclose(input);
                fclose(temporary);
                return 1;
            }
        }
    }

    free(line);
    fclose(input);

    FILE *output = fopen(output_path, "w");
    if (output == NULL) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        fclose(temporary);
        return 1;
    }

    int failed = 0;

    if (has_error) {
        if (fprintf(output, "ERROR:%zu\n", error_position) < 0) {
            failed = 1;
        }
    } else {
        rewind(temporary);

        int ch;
        while ((ch = fgetc(temporary)) != EOF) {
            if (fputc(ch, output) == EOF) {
                failed = 1;
                break;
            }
        }

        if (ferror(temporary)) {
            failed = 1;
        }
    }

    if (fclose(output) != 0) {
        failed = 1;
    }

    fclose(temporary);

    if (failed) {
        fprintf(stderr, "Error writing output file: %s\n", output_path);
        return 1;
    }

    return 0;
}
// Prompt 6: Review my command-line argument handling for file mode,
// directory mode, and output directory options.
int main(int argc, char *argv[])
{
    const char *input_path = NULL;
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    int directory_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc || directory_mode || input_path != NULL) {
                fprintf(stderr, "Invalid directory arguments.\n");
                return 1;
            }

            input_dir = argv[++i];
            directory_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc || output_dir != NULL) {
                fprintf(stderr, "Invalid output directory argument.\n");
                return 1;
            }

            output_dir = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            if (input_path != NULL || directory_mode) {
                fprintf(stderr, "Too many input arguments.\n");
                return 1;
            }

            input_path = argv[i];
        }
    }

    if (argc == 1 || (input_path == NULL && !directory_mode)) {
        fprintf(stderr,
                "Usage: calc [-d DIR] [-o OUTDIR] [input.txt]\n");
        return 1;
    }

    const char *source_name = directory_mode
        ? base_name(input_dir) : input_base_name(input_path);

    if (source_name == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    char *default_output = NULL;

    if (output_dir == NULL) {
        size_t needed = strlen(source_name) + 64;
        default_output = malloc(needed);

        if (default_output == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            if (!directory_mode) {
                free((void *)source_name);
            }
            return 1;
        }

        snprintf(default_output, needed, "%s_nihad_251ADB080",
                 source_name);
        output_dir = default_output;
    }

    if (!directory_mode) {
        free((void *)source_name);
    }

    struct stat st;
    if (stat(output_dir, &st) != 0) {
        if (mkdir(output_dir, 0777) != 0) {
            fprintf(stderr, "Cannot create output directory: %s\n",
                    output_dir);
            free(default_output);
            return 1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Output path is not a directory: %s\n",
                output_dir);
        free(default_output);
        return 1;
    }

    int status = 0;

    if (!directory_mode) {
        char *base = input_base_name(input_path);
        if (base == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            free(default_output);
            return 1;
        }

        size_t name_size = strlen(base) + 64;
        char *output_name = malloc(name_size);

        if (output_name == NULL) {
            free(base);
            free(default_output);
            fprintf(stderr, "Memory allocation failed.\n");
            return 1;
        }

        snprintf(output_name, name_size,
                 "%s_nihad_naghizade_251ADB080.txt", base);

        char *output_path = join_path(output_dir, output_name);

        if (output_path == NULL) {
            fprintf(stderr, "Memory allocation failed.\n");
            status = 1;
        } else {
            status = process_file(input_path, output_path);
        }

        free(output_path);
        free(output_name);
        free(base);
    } else {
        DIR *dir = opendir(input_dir);

        if (dir == NULL) {
            fprintf(stderr, "Cannot open input directory: %s\n",
                    input_dir);
            free(default_output);
            return 1;
        }
        

        struct dirent *entry;

        while ((entry = readdir(dir)) != NULL) {
            size_t name_length = strlen(entry->d_name);

            if (name_length < 4 ||
                strcmp(entry->d_name + name_length - 4, ".txt") != 0) {
                continue;
            }

            char *path = join_path(input_dir, entry->d_name);
            if (path == NULL) {
                fprintf(stderr, "Memory allocation failed.\n");
                status = 1;
                break;
            }

            struct stat input_stat;
            if (stat(path, &input_stat) != 0 ||
                !S_ISREG(input_stat.st_mode)) {
                free(path);
                continue;
            }

            char *base = input_base_name(entry->d_name);
            if (base == NULL) {
                free(path);
                fprintf(stderr, "Memory allocation failed.\n");
                status = 1;
                break;
            }

            size_t name_size = strlen(base) + 64;
            char *output_name = malloc(name_size);

            if (output_name == NULL) {
                free(base);
                free(path);
                fprintf(stderr, "Memory allocation failed.\n");
                status = 1;
                break;
            }

            snprintf(output_name, name_size,
                     "%s_nihad_naghizade_251ADB080.txt", base);

            char *output_path = join_path(output_dir, output_name);

            if (output_path == NULL ||
                process_file(path, output_path) != 0) {
                status = 1;
            }

            free(output_path);
            free(output_name);
            free(base);
            free(path);
        }

        closedir(dir);
    }

    free(default_output);
    return status;
}
// Prompt 7: Help me debug compilation errors and identify problems
// in my C code.
// Prompt 8: Help me verify my implementation using the provided
// functional tests, CLI tests, and sanitizer tests.