#include "rect_splitter.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static const list_node_t list_node_zeroed = {.next = NULL};
static const list_t list_zeroed = {.head = NULL, .tail = NULL};

inline void
rect_init(rect_t *r, int x, int y, int w, int h)
{
    r->area = w * h;

    r->left = x;
    r->top = y;

    r->right = x + w;
    r->bottom = y + h;

    r->width = w;
    r->height = h;
}

void
rect_print(const rect_t r)
{
    printf("<rect(%d, %d, %d, %d)>", r.left, r.top, r.width, r.height);
}

void
rect_list_print(const list_t rects)
{
    list_node_t *node;
    int len;

    len = 0;
    for (node = rects.head; node != NULL; node = node->next)
        len++;

    printf("[");
    for (node = rects.head; node != NULL; node = node->next) {
        rect_print(((rect_node_t *)node)->rect);
        if (node->next) {
            putchar(',');
            if (len < 4)
                putchar(' ');
            else {
                putchar('\n');
                putchar(' ');
            }
        }
    }
    printf("]\n");
}

inline void
rect_list_append_node(list_t *rects, list_node_t *node)
{
    if (rects->tail) {
        rects->tail->next = node;
        rects->tail = node;
    } else {
        rects->head = node;
        rects->tail = node;
    }
}

inline void
rect_list_append(list_t *rects, const rect_t r)
{
    rect_node_t *rect_node;

    rect_node = malloc(sizeof(rect_node_t));
    rect_node->rect = r;
    rect_node->_lst = list_node_zeroed;

    rect_list_append_node(rects, (list_node_t *)rect_node);
}

inline void
rect_list_append_xywh(list_t *rects, int x, int y, int w, int h)
{
    rect_t r;

    rect_init(&r, x, y, w, h);
    rect_list_append(rects, r);
}

inline void
rect_list_concat(list_t *rects, list_t *other)
{
    if (!other->head)
        return;

    if (rects->tail) {
        rects->tail->next = other->head;
        rects->tail = other->tail;
    } else {
        rects->head = other->head;
        rects->tail = other->tail;
    }
    *other = list_zeroed;
}

inline list_node_t *
rect_list_unlink_next(list_t *rects, list_node_t *parent_node)
{
    list_node_t *node;

    if (parent_node) {
        node = parent_node->next;
        parent_node->next = node->next;
    } else {
        node = rects->head;
        rects->head = node->next;
    }

    if (rects->tail == node)
        rects->tail = parent_node;

    *node = list_node_zeroed;

    return node;
}

inline void
rect_list_del_next(list_t *rects, list_node_t *parent_node)
{
    list_node_t *node;

    node = rect_list_unlink_next(rects, parent_node);
    free(node);
}

void
rect_list_clear(list_t *rects)
{
    list_node_t *node;

    node = rects->head;
    while (node) {
        list_node_t *aux;

        aux = node->next;
        free(node);
        node = aux;
    }
    *rects = list_zeroed;
}

static inline void
_calc_intra_rect_area(const rect_t a, const rect_t b, int *width, int *height)
{
    int max_left, min_right, max_top, min_bottom;

    if (a.left < b.left)
        max_left = b.left;
    else
        max_left = a.left;

    if (a.right < b.right)
        min_right = a.right;
    else
        min_right = b.right;

    *width = min_right - max_left;

    if (a.top < b.top)
        max_top = b.top;
    else
        max_top = a.top;

    if (a.bottom < b.bottom)
        min_bottom = a.bottom;
    else
        min_bottom = b.bottom;

    *height = min_bottom - max_top;
}

static inline void
_split_strict(list_t *dirty, const rect_t current, rect_t r)
{
    int h_1, h_2, w_1, w_2;

    h_1 = current.top - r.top;
    h_2 = r.bottom - current.bottom;
    w_1 = current.left - r.left;
    w_2 = r.right - current.right;

    if (h_1 > 0) {
        /*    .--.r (b)                .---.r2
         *    |  |                     |   |
         *  .-------.cur (a) .---.r    '---'
         *  | |  |  |     -> |   |   +
         *  | `--'  |        `---'
         *  `-------'
         */
        rect_list_append_xywh(dirty, r.left, r.top, r.width, h_1);
        r.height -= h_1;
        r.top = current.top;
    }

    if (h_2 > 0) {
        /*  .-------.cur (a)
         *  | .---. |        .---.r
         *  | |   | |    ->  |   |
         *  `-------'        `---'   +  .---.r2
         *    |   |                     |   |
         *    `---'r (b)                `---'
         */
        rect_list_append_xywh(dirty, r.left, current.bottom, r.width, h_2);
        r.height -= h_2;
    }

    if (w_1 > 0) {
        /* (b) r  .----.cur (a)
         *     .--|-.  |      .--.r2   .-.r
         *     |  | |  |  ->  |  |   + | |
         *     `--|-'  |      `--'     `-'
         *        `----'
         */
        rect_list_append_xywh(dirty, r.left, r.top, w_1, r.height);
        /* not necessary to keep these, r (b) will be destroyed */
        /* r.width -= w_1; */
        /* r.left = current.left; */
    }

    if (w_2 > 0) {
        /*  .----.cur (a)
         *  |    |
         *  |  .-|--.r (b)  .-.r   .--.r2
         *  |  | |  |    -> | |  + |  |
         *  |  `-|--'       `-'    `--'
         *  `----'
         */
        rect_list_append_xywh(dirty, current.right, r.top, w_2, r.height);
        /* not necessary to keep this, r (b) will be destroyed */
        /* r.width -= w_2; */
    }
}

void
rect_list_del_split_strict(list_t *rects, const rect_t del_r)
{
    list_t modified = list_zeroed;
    list_node_t *cur_node, *prev_node;

    prev_node = NULL;
    cur_node = rects->head;
    while (cur_node) {
        int intra_width, intra_height;
        rect_t current;

        current = ((rect_node_t*)cur_node)->rect;

        _calc_intra_rect_area(del_r, current, &intra_width, &intra_height);
        if ((intra_width <= 0) || (intra_height <= 0)) {
            /*  .---.current      .---.del_r
             *  |   |             |   |
             *  `---+---.del_r    `---+---.current
             *      |   |             |   |
             *      `---'             `---'
             * no interception, nothing to do
             */
            prev_node = cur_node;
            cur_node = cur_node->next;
        } else if ((intra_width == current.width) &&
                   (intra_height == current.height)) {
            /*  .-------.del_r
             *  | .---. |
             *  | |   | |
             *  | `---'current
             *  `-------'
             * current is contained, remove from rects
             */
            cur_node = cur_node->next;
            rect_list_del_next(rects, prev_node);
        } else {
            _split_strict(&modified, del_r, current);
            cur_node = cur_node->next;
            rect_list_del_next(rects, prev_node);
        }
    }

    rect_list_concat(rects, &modified);
}

void
rect_list_add_split_strict(list_t *rects, list_node_t *node)
{
    list_t dirty = list_zeroed;
    list_t new_dirty = list_zeroed;
    list_node_t *cur_node;

    if (!rects->head) {
        rect_list_append_node(rects, node);
        return;
    }

    rect_list_append_node(&dirty, node);

    cur_node = rects->head;
    while (dirty.head) {
        rect_t current;

        if (!cur_node) {
            rect_list_concat(rects, &dirty);
            break;
        }

        current = ((rect_node_t*)cur_node)->rect;

        while (dirty.head) {
            int intra_width, intra_height;
            rect_t r;

            r = ((rect_node_t *)dirty.head)->rect;
            _calc_intra_rect_area(r, current, &intra_width, &intra_height);
            if (intra_width == r.width && intra_height == r.height)
                /*  .-------.cur
                 *  | .---.r|
                 *  | |   | |
                 *  | `---' |
                 *  `-------'
                 */
                rect_list_del_next(&dirty, NULL);
            else if (intra_width <= 0 || intra_height <= 0) {
                /*  .---.cur     .---.r
                 *  |   |        |   |
                 *  `---+---.r   `---+---.cur
                 *      |   |        |   |
                 *      `---'        `---'
                 */
                list_node_t *tmp;
                tmp = rect_list_unlink_next(&dirty, NULL);
                rect_list_append_node(&new_dirty, tmp);
            } else {
                _split_strict(&new_dirty, current, r);
                rect_list_del_next(&dirty, NULL);
            }
        }
        dirty = new_dirty;
        new_dirty = list_zeroed;

        cur_node = cur_node->next;
    }
}

static inline void
_calc_intra_outer_rect_area(const rect_t a, const rect_t b,
                            rect_t *intra, rect_t *outer)
{
    int min_left, max_left, min_right, max_right;
    int min_top, max_top, min_bottom, max_bottom;

    if (a.left < b.left) {
        max_left = b.left;
        min_left = a.left;
    } else {
        max_left = a.left;
        min_left = b.left;
    }

    if (a.right < b.right) {
        min_right = a.right;
        max_right = b.right;
    } else {
        min_right = b.right;
        max_right = a.right;
    }

    intra->left = max_left;
    intra->right = min_right;
    intra->width = min_right - max_left;

    outer->left = min_left;
    outer->right = max_right;
    outer->width = max_right - min_left;

    if (a.top < b.top) {
        max_top = b.top;
        min_top = a.top;
    } else {
        max_top = a.top;
        min_top = b.top;
    }

    if (a.bottom < b.bottom) {
        min_bottom = a.bottom;
        max_bottom = b.bottom;
    } else {
        min_bottom = b.bottom;
        max_bottom = a.bottom;
    }

    intra->top = max_top;
    intra->bottom = min_bottom;
    intra->height = min_bottom - max_top;
    if (intra->width > 0 && intra->height > 0)
        intra->area = intra->width * intra->height;
    else
        intra->area = 0;

    outer->top = min_top;
    outer->bottom = max_bottom;
    outer->height = max_bottom - min_top;
    outer->area = outer->width * outer->height;
}

enum {
    SPLIT_FUZZY_ACTION_NONE,
    SPLIT_FUZZY_ACTION_SPLIT,
    SPLIT_FUZZY_ACTION_MERGE
};

static inline int
_split_fuzzy(list_t *dirty, const rect_t a, rect_t *b)
{
    int h_1, h_2, w_1, w_2, action;

    h_1 = a.top - b->top;
    h_2 = b->bottom - a.bottom;
    w_1 = a.left - b->left;
    w_2 = b->right - a.right;

    action = SPLIT_FUZZY_ACTION_NONE;

    if (h_1 > 0) {
        /*    .--.r (b)                .---.r2
         *    |  |                     |   |
         *  .-------.cur (a) .---.r    '---'
         *  | |  |  |     -> |   |   +
         *  | `--'  |        `---'
         *  `-------'
         */
        rect_list_append_xywh(dirty, b->left, b->top, b->width, h_1);
        b->height -= h_1;
        b->top = a.top;
        action = SPLIT_FUZZY_ACTION_SPLIT;
    }

    if (h_2 > 0) {
        /*  .-------.cur (a)
         *  | .---. |        .---.r
         *  | |   | |    ->  |   |
         *  `-------'        `---'   +  .---.r2
         *    |   |                     |   |
         *    `---'r (b)                `---'
         */
        rect_list_append_xywh(dirty, b->left, a.bottom, b->width, h_2);
        b->height -= h_2;
        action = SPLIT_FUZZY_ACTION_SPLIT;
    }

    if ((w_1 > 0 || w_2 > 0) && a.height == b->height)
        return SPLIT_FUZZY_ACTION_MERGE;

    if (w_1 > 0) {
        /* (b)  r  .----.cur (a)
         *      .--|-.  |      .--.r2   .-.r
         *      |  | |  |  ->  |  |   + | |
         *      `--|-'  |      `--'     `-'
         *         `----'
         */
        rect_list_append_xywh(dirty, b->left, b->top, w_1, b->height);
        /* not necessary to keep these, r (b) will be destroyed */
        /* b->width -= w_1; */
        /* b->left = a.left; */
        action = SPLIT_FUZZY_ACTION_SPLIT;
    }

    if (w_2 > 0) {
        /* .----.cur (a)
         * |    |
         * |  .-|--.r (b)  .-.r   .--.r2
         * |  | |  |    -> | |  + |  |
         * |  `-|--'       `-'    `--'
         * `----'
         */
        rect_list_append_xywh(dirty, a.right, b->top, w_2, b->height);
        /* not necessary to keep these, r (b) will be destroyed */
        /* b->width -= w_2; */
        action = SPLIT_FUZZY_ACTION_SPLIT;
    }

    return action;
}

list_node_t *
rect_list_add_split_fuzzy(list_t *rects, list_node_t *node, int accepted_error)
{
    list_t dirty = list_zeroed;
    list_node_t *old_last;

    old_last = rects->tail;

    if (!rects->head) {
        rect_list_append_node(rects, node);
        return old_last;
    }

    rect_list_append_node(&dirty, node);
    while (dirty.head) {
        list_node_t *d_node, *cur_node, *prev_cur_node;
        int keep_dirty;
        rect_t r;

        d_node = rect_list_unlink_next(&dirty, NULL);
        r = ((rect_node_t *)d_node)->rect;

        prev_cur_node = NULL;
        cur_node = rects->head;
        keep_dirty = 1;
        while (cur_node) {
            int area, action;
            rect_t current, intra, outer;

            current = ((rect_node_t *)cur_node)->rect;

            _calc_intra_outer_rect_area(r, current, &intra, &outer);
            area = current.area + r.area - intra.area;

            if (intra.width == r.width && intra.height == r.height) {
                /*  .-------.cur
                 *  | .---.r|
                 *  | |   | |
                 *  | `---' |
                 *  `-------'
                 */
                keep_dirty = 0;
                break;
            } else if (intra.width == current.width &&
                       intra.height == current.height) {
                /* .-------.r
                 * | .---.cur
                 * | |   | |
                 * | `---' |
                 * `-------'
                 */
                if (old_last == cur_node)
                    old_last = prev_cur_node;
                cur_node = cur_node->next;
                rect_list_del_next(rects, prev_cur_node);
            } else if (outer.area - area <= accepted_error) {
                /* .-----------. bounding box (outer)
                 * |.---. .---.|
                 * ||cur| |r  ||
                 * ||   | |   ||
                 * |`---' `---'|
                 * `-----------'
                 * merge them, remove both and add merged
                 */
                rect_node_t *n;

                if (old_last == cur_node)
                    old_last = prev_cur_node;

                n = (rect_node_t *)rect_list_unlink_next(rects, prev_cur_node);
                n->rect = outer;
                rect_list_append_node(&dirty, (list_node_t *)n);

                keep_dirty = 0;
                break;
            } else if (intra.area <= accepted_error) {
                /*  .---.cur     .---.r
                 *  |   |        |   |
                 *  `---+---.r   `---+---.cur
                 *      |   |        |   |
                 *      `---'        `---'
                 *  no split, no merge
                 */
                prev_cur_node = cur_node;
                cur_node = cur_node->next;
            } else {
                /* split is required */
                action = _split_fuzzy(&dirty, current, &r);
                if (action == SPLIT_FUZZY_ACTION_MERGE) {
                    /* horizontal merge is possible: remove both, add merged */
                    rect_node_t *n;

                    if (old_last == cur_node)
                        old_last = prev_cur_node;

                    n = (rect_node_t *)
                        rect_list_unlink_next(rects, prev_cur_node);

                    n->rect.left = outer.left;
                    n->rect.width = outer.width;
                    n->rect.right = outer.right;
                    n->rect.area = outer.width * r.height;
                    rect_list_append_node(&dirty, (list_node_t *)n);
                } else if (action == SPLIT_FUZZY_ACTION_NONE) {
                    /*
                     * this rect check was totally useless,
                     * should never happen
                     */
                    /* prev_cur_node = cur_node; */
                    /* cur_node = cur_node->next; */
                    assert(1 == 0 && "Should not reach.");
                }

                keep_dirty = 0;
                break;
            }
        }

        if (keep_dirty)
            rect_list_append_node(rects, d_node);
        else
            free(d_node);
    }

    return old_last;
}

static inline void
_calc_outer_rect_area(const rect_t a, const rect_t b,
                      rect_t *outer)
{
    int min_left, max_right;
    int min_top, max_bottom;

    if (a.left < b.left)
        min_left = a.left;
    else
        min_left = b.left;

    if (a.right < b.right)
        max_right = b.right;
    else
        max_right = a.right;

    outer->left = min_left;
    outer->right = max_right;
    outer->width = max_right - min_left;

    if (a.top < b.top)
        min_top = a.top;
    else
        min_top = b.top;

    if (a.bottom < b.bottom)
        max_bottom = b.bottom;
    else
        max_bottom = a.bottom;

    outer->top = min_top;
    outer->bottom = max_bottom;
    outer->height = max_bottom - min_top;

    outer->area = outer->width * outer->height;
}

void
rect_list_merge_rects(list_t *rects, list_t *to_merge, int accepted_error)
{
    while (to_merge->head) {
        list_node_t *node, *parent_node;
        rect_t r1;
        int merged;

        r1 = ((rect_node_t *)to_merge->head)->rect;

        merged = 0;
        parent_node = NULL;
        node = rects->head;
        while (node != NULL) {
            rect_t r2, outer;
            int area;

            r2 = ((rect_node_t *)node)->rect;

            _calc_outer_rect_area(r1, r2, &outer);
            area = r1.area + r2.area; /* intra area is taken as 0 */
            if (outer.area - area <= accepted_error) {
                /*
                 * remove both r1 and r2, create r3
                 * actually r3 uses r2 instance, saves memory
                 */
                rect_node_t *n;

                n = (rect_node_t *)rect_list_unlink_next(rects, parent_node);
                n->rect = outer;
                rect_list_append_node(to_merge, (list_node_t *)n);
                merged = 1;
                break;
            }

            parent_node = node;
            node = node->next;
        }

        if (!merged) {
            list_node_t *n;
            n = rect_list_unlink_next(to_merge, NULL);
            rect_list_append_node(rects, n);
        } else
            rect_list_del_next(to_merge, NULL);
    }
}

void
rect_list_add_split_fuzzy_and_merge(list_t *rects, list_node_t *node,
                                    int split_accepted_error,
                                    int merge_accepted_error)
{
    list_node_t *n;

    n = rect_list_add_split_fuzzy(rects, node, split_accepted_error);
    if (n && n->next) {
        list_t to_merge;

        /* split list into 2 segments, already merged and to merge */
        to_merge.head = n->next;
        to_merge.tail = rects->tail;
        rects->tail = n;
        n->next = NULL;

        rect_list_merge_rects(rects, &to_merge, merge_accepted_error);
    }
}
