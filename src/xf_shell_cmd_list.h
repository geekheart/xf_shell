/**
 * @file xf_shell_cmd_list.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-02-20
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_SHELL_CMD_LIST_H__
#define __XF_SHELL_CMD_LIST_H__

/* ==================== [Includes] ========================================== */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

// 获取父对象的地址
#ifndef GET_PARENT_ADDR
#define GET_PARENT_ADDR(me, parent, my_name) \
    ((parent *)((char *)(me) - (ptrdiff_t) & ((parent *)0)->my_name))
#endif

/* ==================== [Typedefs] ========================================== */

typedef struct _xf_cmd_list_t {
    struct _xf_cmd_list_t *next;
    struct _xf_cmd_list_t *prev;
} xf_cmd_list_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 获取指定节点的前驱节点。
 *
 * @param[in] list 当前节点。
 * @return 前驱节点指针。
 */
static inline xf_cmd_list_t *xf_cmd_list_get_prev(xf_cmd_list_t *list)
{
    return list->prev;
}

/**
 * @brief 获取指定节点的后继节点。
 *
 * @param[in] list 当前节点。
 * @return 后继节点指针。
 */
static inline xf_cmd_list_t *xf_cmd_list_get_next(xf_cmd_list_t *list)
{
    return list->next;
}

/**
 * @brief 设置指定节点的前驱节点指针。
 *
 * @param[in,out] list 当前节点。
 * @param[in] other 新的前驱节点。
 */
static inline void xf_cmd_list_set_prev(xf_cmd_list_t *list, xf_cmd_list_t *other)
{
    list->prev = other;
}

/**
 * @brief 设置指定节点的后继节点指针。
 *
 * @param[in,out] list 当前节点。
 * @param[in] other 新的后继节点。
 */
static inline void xf_cmd_list_set_next(xf_cmd_list_t *list, xf_cmd_list_t *other)
{
    list->next = other;
}

/**
 * @brief 初始化一个循环双向链表节点。
 *
 * @details
 * 初始化后节点处于“独立”状态：`next == prev == self`。
 *
 * @param[in,out] list 待初始化节点。
 */
static inline void xf_cmd_list_init(xf_cmd_list_t *list)
{
    xf_cmd_list_set_prev(list, list);
    xf_cmd_list_set_next(list, list);
}

/**
 * @brief 将链表 `node2` 追加到链表 `node1` 尾部。
 *
 * @details
 * 两个参数均应为循环链表头节点。连接完成后形成一个新的循环链表。
 *
 * @param[in,out] node1 目标链表头。
 * @param[in,out] node2 被追加链表头。
 */
static inline void xf_cmd_list_attach(xf_cmd_list_t *node1, xf_cmd_list_t *node2)
{
    xf_cmd_list_t *tail1 = xf_cmd_list_get_prev(node1);
    xf_cmd_list_t *tail2 = xf_cmd_list_get_prev(node2);
    xf_cmd_list_set_next(tail1, node2);
    xf_cmd_list_set_next(tail2, node1);
    xf_cmd_list_set_prev(node1, tail2);
    xf_cmd_list_set_prev(node2, tail1);
}

/**
 * @brief 将链表 `node2` 插入到链表 `node1` 头部（头插）。
 *
 * @details
 * 两个参数均应为循环链表头节点。连接完成后形成一个新的循环链表。
 *
 * @param[in,out] node1 目标链表头。
 * @param[in,out] node2 被插入链表头。
 */
static inline void xf_cmd_list_head_attach(xf_cmd_list_t *node1, xf_cmd_list_t *node2)
{
    xf_cmd_list_t *head1 = xf_cmd_list_get_next(node1);
    xf_cmd_list_t *head2 = xf_cmd_list_get_next(node2);
    xf_cmd_list_set_prev(head1, node2);
    xf_cmd_list_set_prev(head2, node1);
    xf_cmd_list_set_next(node1, head2);
    xf_cmd_list_set_next(node2, head1);
}

/**
 * @brief 将节点从其所在循环链表中摘除。
 *
 * @details
 * 摘除后该节点会被重置为独立循环节点：`next == prev == self`。
 *
 * @param[in,out] node 待摘除节点。
 */
static inline void xf_cmd_list_detach(xf_cmd_list_t *node)
{
    xf_cmd_list_t *next = xf_cmd_list_get_next(node);
    xf_cmd_list_t *prev  = xf_cmd_list_get_prev(node);
    xf_cmd_list_set_next(prev, next);
    xf_cmd_list_set_next(node, node);
    xf_cmd_list_set_prev(node, node);
    xf_cmd_list_set_prev(next, prev);
}

/**
 * @brief 判断循环链表是否为空。
 *
 * @param[in] list 链表头节点。
 * @return
 * - `1`：空链表；
 * - `0`：非空链表。
 */
static inline int xf_cmd_list_is_empty(xf_cmd_list_t *list)
{
    return (xf_cmd_list_get_next(list) == list);
}

/**
 * @brief 统计循环链表中有效节点数量。
 *
 * @param[in] list 链表头节点。
 * @return 链表节点数量（不含头节点自身）。
 */
static inline int xf_cmd_list_size(xf_cmd_list_t *list)
{
    int n = 0;
    xf_cmd_list_t *next;
    for (next = xf_cmd_list_get_next(list); next != list; next = xf_cmd_list_get_next(next)) {
        ++n;
    }
    return n;
}

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __XF_SHELL_CMD_LIST_H__
