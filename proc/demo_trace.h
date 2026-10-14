/* SPDX-License-Identifier: GPL-2.0 */

/* 定义trace event所属子系统：/sys/kernel/tracing/events/proc_demo
 * 并可通过bpftrace对应的tracepoint:proc_demo:xxx跟踪
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM proc_demo

/* 内核会通过定义宏 TRACE_HEADER_MULTI_READ 多次包含该头文件，每次
 * 给TRACE_EVENT, TP_PROTO, __field等宏赋予不同的含义，用它们生成
 * 不同的代码
 */
#if !defined(_TRACE_PROC_DEMO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PROC_DEMO_H

#include <linux/tracepoint.h>

/* 定义跟踪事件：demo_item_show
 *
 */
TRACE_EVENT(
    /* 事件名称，会生成一个trace_demo_item_show(...)
     * 并创建tracefs事件：/sys/kernel/tracing/events/proc_demo/demo_item_show
     * bpftrace名称则是：tracepoint:proc_demo_demo_item_show
     */
    demo_item_show,  

    /* 定义调用接口
     *  描述tracepoint调用函数的参数类型，使它生成的函数类似于：
     *  void trace_demo_item_show(unsigned int id, const char *name, long value)
     */
    TP_PROTO(unsigned int id, const char *name, long value), 

    /* 指定调用时传递的参数
     *  必须与TP_PROTO匹配
     */
    TP_ARGS(id, name, value),  // 函数参数名称
    
    /* 定义事件真正保存的字段：定义一条trace event记录里面存储什么
     * 它类似于一个结构，里面保存的是成员
     *  对于固定大小的整数、指针或枚举通常使用__field(...)定义
     *  __string(...)定义一个动态长度字符串字段，格式为：
     *      __string(事件字段名，计算字符长度时使用的表达式)
     */
    TP_STRUCT__entry( // 事件记录结构
        __field(unsigned int, id)
        __string(name, name)
        __field(long, value)
    ), 
    
    /* 把调用参数复制进事件记录
     *  _entry指向内核trace框架为当前事件分配的记录空间，它的结构类型参照上面的"事件记录结构"
     *  __assign_str(事件字段名，源字符串表达式)
     */
    TP_fast_assign(  // 字段赋值
        __entry->id = id;
        __assign_str(name, name);
        __entry->value = value;
    ), 

    /* 定义传统文本输出格式，即输出到/sys/kernel/tracing/trace_pipe中的内容
     */
    TP_printk("id=%u name=%s value=%ld", __entry->id, __get_str(name), __entry->value)
);

#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE demo_trace

/* 生成真正的定义
 */
#include <trace/define_trace.h>
