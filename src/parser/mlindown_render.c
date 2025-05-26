#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "parser/mlindown_render.h"

typedef struct {
    char *buf;
    size_t len, cap;
} HtmlBuf;

static void buf_init(HtmlBuf *b) {
    b->cap = 1024;
    b->len = 0;
    b->buf = malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
}

static void buf_grow(HtmlBuf *b, size_t needed) {
    if (needed <= b->cap) return;
    while (b->cap < needed) b->cap *= 2;
    b->buf = realloc(b->buf, b->cap);
}

static void buf_printf(HtmlBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int avail = (int)(b->cap - b->len);
    int needed = vsnprintf(b->buf + b->len, avail, fmt, ap);
    va_end(ap);
    if (needed < 0) return;
    if ((size_t)needed >= (size_t)avail) {
        buf_grow(b, b->len + needed + 1);
        va_start(ap, fmt);
        vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
        va_end(ap);
    }
    b->len += needed;
}

static void render_node_str(const Node *node, HtmlBuf *b) {
    if (!node) return;

    switch (node->type) {
    case MLINDOWN_NODE_DOCUMENT:
        for (size_t i = 0; i < node->child_count; i++)
            render_node_str(node->children[i], b);
        break;

    case MLINDOWN_NODE_HEADING:
        buf_printf(b, "<h%d>%s</h%d>\n\n",
                   node->level,
                   node->text ? node->text : "",
                   node->level);
        break;

    case MLINDOWN_NODE_PARAGRAPH:
        buf_printf(b, "<p>%s</p>\n\n",
                   node->text ? node->text : "");
        break;

    case MLINDOWN_NODE_LIST:
        buf_printf(b, "<ul>\n");
        for (size_t i = 0; i < node->child_count; i++)
            render_node_str(node->children[i], b);
        buf_printf(b, "</ul>\n\n");
        break;

    case MLINDOWN_NODE_LIST_ITEM:
        buf_printf(b, "  <li>%s</li>\n",
                   node->text ? node->text : "");
        break;

    default:
        /* ignore */
        break;
    }
}

char *render_html_str(const Node *node) {
    HtmlBuf buf;
    buf_init(&buf);
    if (!buf.buf) return NULL;

    render_node_str(node, &buf);
    /* shrink to fit */
    buf.buf = realloc(buf.buf, buf.len + 1);
    return buf.buf;
}

void render_html(const Node *node, FILE *out) {
    if (!node) return;

    switch (node->type) {
        case MLINDOWN_NODE_DOCUMENT:
            for (size_t i = 0; i < node->child_count; i++) {
                render_html(node->children[i], out);
            }
            break;

        case MLINDOWN_NODE_HEADING:
            // <h1>…</h1> up to <h6>
            fprintf(out, "<h%d>%s</h%d>\n\n",
                    node->level,
                    node->text ? node->text : "",
                    node->level);
            break;

        case MLINDOWN_NODE_PARAGRAPH:
            fprintf(out, "<p>%s</p>\n\n",
                    node->text ? node->text : "");
            break;

        case MLINDOWN_NODE_LIST:
            fprintf(out, "<ul>\n");
            for (size_t i = 0; i < node->child_count; i++) {
                render_html(node->children[i], out);
            }
            fprintf(out, "</ul>\n\n");
            break;

        case MLINDOWN_NODE_LIST_ITEM:
            fprintf(out, "  <li>%s</li>\n",
                    node->text ? node->text : "");
            break;

        default:
            break;
    }
}