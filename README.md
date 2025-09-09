# Linux Project — Online Collaborative Drawing Platform

## Overview
Inspired by **Reddit r/place**, this project implements an online collaborative drawing platform.  
It allows users to co-create pixel art in real time on a shared canvas, encouraging creativity and interaction.  

Main features:
- Real-time collaborative editing  
- Multithreaded high-concurrency processing  
- Full-duplex WebSocket communication  
- Pixel-level rendering with HTML5 Canvas  

---

## Key Features
1. **Dynamic Gradient Background**: Smooth CSS gradient animations  
2. **64-Color Compression**: Efficient integer-to-RGB mapping with bitwise operations  
3. **Logging & Signal Handling**: Redirected logs; graceful shutdown on SIGINT  
4. **Pixel Highlight Blinking**: Flashing effect for selected pixels to enhance interaction  
5. **Double Transaction Linked List**: Reduced lock contention, higher concurrency performance  

---

## Architecture
- **Multithreading Model**  
  - Worker Thread: Handles pixel update transactions  
  - Update Thread: Generates JSON data and saves pixel snapshots  
  - Cleaner Thread: Removes inactive WebSocket clients  
  - Test Thread: Simulates heavy load for stress testing  

- **I/O Multiplexing**  
  Implemented with `epoll` for efficient handling of massive concurrent connections.  

- **Frontend Rendering**  
  HTML5 **Canvas** for pixel-level drawing and real-time rendering.  

---

## Protocols
- **HTTP**: Static content delivery  
- **WebSocket**: Real-time synchronization of pixel data  
- **POST Requests**: Ensures updates even if WebSocket disconnects  
- **Fast Reconnection**: Maintains client liveness for stability  

---

## Performance Testing
- **100 user threads**: 19,100 transactions processed in 3 minutes, success rate **99.97%**  
- **3,000 user threads**: 919,000+ transactions processed in 27 seconds, success rate **99.67%**  
- System remained **stable under high load** and performed graceful shutdown.  

---

## Server Optimization
- **Thread Detach (pthread_detach)**: Reduces thread dependency  
- **Static & Dynamic Resource Caching**: Less file I/O, faster response  
- **Connection Cleanup**: Periodic removal of zombie clients to optimize resources  


# Linux项目 —— 在线协作绘画平台

## 项目简介
本项目灵感来源于 **Reddit r/place** 活动，目标是实现一个在线协作绘画平台。用户可以在同一画布上实时绘制像素图，通过互动体验社区创作的乐趣。  

平台支持：
- 实时协作绘制  
- 多线程高并发处理  
- WebSocket 全双工通信  
- 像素级 Canvas 前端渲染  

---

## 功能亮点
1. **动态渐变背景**：CSS 渐变 + 动画实现平滑视觉效果  
2. **64色压缩编码**：使用位运算将整数映射至 RGB，提升存储效率  
3. **日志与信号管理**：输出重定向至日志文件；支持 SIGINT 优雅关闭  
4. **像素闪烁高亮**：前端像素选中闪烁，提升用户交互体验  
5. **双事务链表**：降低锁竞争，提升高并发性能  

---

## 系统架构
- **多线程模型**  
  - 工作线程：处理像素更新事务  
  - 更新线程：定期生成 JSON 数据并保存文件  
  - 清理线程：定期移除不活跃连接  
  - 用户测试线程：模拟高负载压力测试  

- **I/O 多路复用**  
  使用 `epoll` 实现高并发连接管理，单线程高效处理大量客户端。  

- **前端绘制**  
  使用 HTML5 **Canvas** 渲染像素画，支持实时更新和像素级操作。  

---

## 协议设计
- **HTTP**：提供网页加载与静态资源传输  
- **WebSocket**：实现像素画的实时同步广播  
- **POST 请求**：保证 WebSocket 断开时仍能更新像素数据  
- **快速重连机制**：维护活跃状态，提升可靠性  

---

## 性能测试
- **100 用户线程**：3 分钟内处理 19100 个事务，事务受理率 99.97%  
- **3000 用户线程**：27 秒内处理 91 万+ 事务，事务受理率 99.67%  
- 结果表明系统在高负载下仍能 **稳定运行并优雅退出**。  

---

## 服务器优化
- **线程分离 (pthread_detach)**：降低依赖性  
- **静态/动态内容缓存**：减少文件 I/O，提高响应速度  
- **定时清理机制**：移除僵尸连接，保障资源利用率  


