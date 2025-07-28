/**
 * @file xf_cmd_list.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2024-02-20
 *
 * @copyright Copyright (c) 2024, CorAL. All rights reserved.
 *
 */

#ifndef __XF_CMD_LIST_H__
#define __XF_CMD_LIST_H__

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
 * @brief 获取上一个节点
 *
 * @param list 当前节点链表对象
 * @return xf_cmd_list_t* 上一个节点链表对象
 */
static inline xf_cmd_list_t *xf_cmd_list_get_prev(xf_cmd_list_t *list)
{
    return list->prev;
}

/**
 * @brief 获取下一个节点
 *
 * @param list 当前节点链表对象
 * @return xf_cmd_list_t* 下一个节点链表对象
 */
static inline xf_cmd_list_t *xf_cmd_list_get_next(xf_cmd_list_t *list)
{
    return list->next;
}

/**
 * @brief 设置上一个节点
 *
 * @param list 当前节点链表对象
 * @param other 上一个节点的链表对象
 */
static inline void xf_cmd_list_set_prev(xf_cmd_list_t *list, xf_cmd_list_t *other)
{
    list->prev = other;
}

/**
 * @brief 设置下一个节点
 *
 * @param list 当前节点链表对象
 * @param other 下一个节点的链表对象
 */
static inline void xf_cmd_list_set_next(xf_cmd_list_t *list, xf_cmd_list_t *other)
{
    list->next = other;
}

/**
 * @brief 链表初始化
 *
 * @param list 当前节点链表对象
 */
static inline void xf_cmd_list_init(xf_cmd_list_t *list)
{
    xf_cmd_list_set_prev(list, list);
    xf_cmd_list_set_next(list, list);
}

/**
 * @brief 连接两个链表（尾插）
 *
 * @param node1 需要连接的链表1对象
 * @param node2 需要连接的链表2对象
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
 * @brief 连接两个链表（头插）
 *
 * @param node1 需要连接的链表1对象
 * @param node2 需要连接的链表2对象
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
 * @brief 将一个链表从循环链表中断开
 *
 * @param node 需要断开的链表节点
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
 * @brief 判断链表是否是空
 *
 * @param list 当前节点链表对象
 * @return int 返回0，是非空。返回1，是空。
 */
static inline int xf_cmd_list_is_empty(xf_cmd_list_t *list)
{
    return (xf_cmd_list_get_next(list) == list);
}

/**
 * @brief 获取链表大小
 *
 * @param list 当前节点链表对象
 * @return int 当前链表的节点数目
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

#endif // __XF_CMD_LIST_H__
