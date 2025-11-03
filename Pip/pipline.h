#ifndef PIPLINE_H
#define PIPLINE_H
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <windows.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <QSemaphore>
#include <QFileDialog>
#include<qlabel.h>
#include <mutex>
#include "class.h"
using namespace std;
using namespace cv;

typedef enum PIPE_TYPE_E {
    PIPE_SOURCE_E = 0,
    PIPE_PROCESS_E,
    PIPE_SINK_E
} PIPE_TYPE_E;





class AbstractPipe {
public:
    AbstractPipe(string name, PIPE_TYPE_E pipeType)
        : m_pipeName(name), m_pipeType(pipeType), m_exit(false)
    {}

    virtual void pipe(QSemaphore &inSem, QSemaphore &outSem) = 0;

    void setInImage(void* pInImage) {
        m_pInImage = pInImage;
    }

    void setOutImage(void* pOutImage) {
        m_pOutImage = pOutImage;
    }

    bool exit() {
        return m_exit;
    }

    void setExit(bool flag) {
        m_exit = flag;
    }
    bool isPaused(){
        return m_paused;
    }
    void setPaused(bool flag){
        m_paused = flag;
    }

protected:

    string m_pipeName;
    PIPE_TYPE_E m_pipeType;
    bool m_exit;
    void* m_pInImage;
    void* m_pOutImage;
    bool m_paused = false;  //暂停标志位
};


class pipline
{
public:
    pipline();
    static void add_process_modles(AbstractPipe * pipe)
    {
        m_pipe_processes.push_back(pipe);
    }

    static void remove_process_modles(AbstractPipe * pipe)
    {
        auto it = find(m_pipe_processes.begin(), m_pipe_processes.end(), pipe);
        if (it != (m_pipe_processes.end())) {
            m_pipe_processes.erase(it);


            std::cout<<"shanchuok"<<std::endl;
        }
    }

    static void creat_capturepip(AbstractPipe * pipe, bool startPaused )
    {

        m_pipe0 = pipe;
        m_processInSem[0].tryAcquire();
        m_pipe0->setExit(false);
        m_pipe0->setPaused(startPaused);
        m_pipe0->setOutImage(&m_FrameImages[0]);
        m_t0 = new thread(&AbstractPipe::pipe, m_pipe0, ref(m_dummySem), ref(m_processInSem[0]));
    }


    static void createPipeLine()
    {

        //初始化信号量
        m_dummySem.tryAcquire();

        for (size_t i = 0; i < 5; ++i) {
            m_processInSem[i].tryAcquire();
            m_processOutSem[i].tryAcquire();
        }


        // 创建处理管道线程
        for (size_t i = 0; i < m_pipe_processes.size(); ++i) {
            AbstractPipe* pipe = m_pipe_processes[i];
            pipe->setExit(false);
            pipe->setInImage(&m_FrameImages[i]);
            pipe->setOutImage(&m_FrameImages[i+1]);

            m_threads_processes.push_back(
                new thread(&AbstractPipe::pipe, pipe,
                           ref(m_processInSem[i]),
                           ref(m_processInSem[i+1]))
                );
        }
    }


    static void deleteALLPip()
    {

        for (size_t i = 0; i < m_pipe_processes.size(); ++i) {
            if(m_pipe_processes[i]){
                m_pipe_processes[i]->setExit(true);
            }
        }

        if (m_pipe1)
            m_pipe1->setExit(true);


        for (size_t i = 0; i < m_threads_processes.size(); ++i) {
            // 确保线程对象可等待
            if (m_threads_processes[i] && m_threads_processes[i]->joinable()) {
                m_threads_processes[i]->join();
                delete m_threads_processes[i];
            }
        }

        m_threads_processes.clear();

        for (int i = 0; i < 6; i++) {
            m_processInSem[i].release();
            m_processOutSem[i].release();
        }

        if (m_pipe1) {
            // 确保不会在已删除的线程上调用 join
            if (m_t1 && m_t1->joinable())
                m_t1->join();

            // 删除线程对象
            delete m_t1;
            m_t1 = nullptr;

        }
    }
    static void updateProcessModule(AbstractPipe* pipe, bool add) {
        if (!pipe) {
            qCritical() << "错误：传入的处理模块为空";
            return;
        }

        // qDebug() << "开始" << (add ? "添加" : "移除") << "处理模块: " << pipe->getName();

        // 如果管道正在运行，先暂停
        bool wasPaused = false;
        for (auto p : m_pipe_processes) {
            if (p && p->isPaused()) {
                wasPaused = true;
                break;
            }
        }

        if (!wasPaused) {
            qDebug() << "暂停管道";
            pausePipeLine(); // 暂停所有管道
        }

        if (add) {
            // 检查模块是否已存在
            auto it = find(m_pipe_processes.begin(), m_pipe_processes.end(), pipe);
            if (it == m_pipe_processes.end()) {
                qDebug() << "添加模块到管道";
                m_pipe_processes.push_back(pipe);
            } else {
                qWarning() << "模块已存在，无需重复添加";
            }
        } else {
            // 移除模块
            auto it = find(m_pipe_processes.begin(), m_pipe_processes.end(), pipe);
            if (it != m_pipe_processes.end()) {
                qDebug() << "从管道中移除模块";
                m_pipe_processes.erase(it);
            } else {
                qWarning() << "模块不存在，无法移除";
            }
        }

        // 重新配置管道，但不完全重建
        qDebug() << "重新配置管道";
        reconfigurePipeLine();

        // 如果之前没有暂停，恢复管道
        if (!wasPaused) {
            qDebug() << "恢复管道";
            resumePipeLine();
        }

        qDebug() << "完成" << (add ? "添加" : "移除") << "处理模块";
    }
    static void reconfigurePipeLine() {
        // 更新处理模块的输入输出图像
        for (size_t i = 0; i < m_pipe_processes.size(); ++i) {
            AbstractPipe* pipe = m_pipe_processes[i];
            pipe->setInImage(&m_Images[i]);
            pipe->setOutImage(&m_Images[i+1]);
        }

        // 释放末端信号量，确保管道能继续流动
        if (!m_pipe_processes.empty()) {
            m_processInSem[m_pipe_processes.size()].release();
        }
    }


    // 移除所有处理模块但保持管道结构
    static void removeAllProcessModules() {
        // 暂停所有管道
        pausePipeLine();

        // 清空处理模块列表
        m_pipe_processes.clear();

        // 释放信号量以防止死锁
        for (int i = 0; i < 6; i++) {
            m_processInSem[i].release();
        }

        std::cout << "所有处理模块已移除" << std::endl;
    }

    static void safeDeletePipeline() {
        try {
            // 先暂停所有线程
            for (size_t i = 0; i < m_pipe_processes.size(); ++i) {
                if(m_pipe_processes[i]){
                    m_pipe_processes[i]->setPaused(true);
                    // 给线程一点时间来响应暂停
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    m_pipe_processes[i]->setExit(true);
                }
            }

            // 释放所有信号量以防止死锁
            for (int i = 0; i < 6; i++) {
                m_processInSem[i].release();
                m_processOutSem[i].release();
            }
            m_dummySem.release();

            // 等待线程结束，但设置超时
            for (size_t i = 0; i < m_threads_processes.size(); ++i) {
                if (m_threads_processes[i] && m_threads_processes[i]->joinable()) {
                    // 使用一个单独的线程来join，这样可以设置超时
                    std::thread joinThread([&]() {
                        m_threads_processes[i]->join();
                    });

                    // 给join线程一个超时时间
                    if (joinThread.joinable()) {
                        joinThread.join();
                    } else {
                        // 如果join线程无法结束，我们这里不能做太多
                        std::cerr << "无法正常结束线程 " << i << std::endl;
                    }
                }
            }

            // 清空处理模块和线程
            m_pipe_processes.clear();
            m_threads_processes.clear();

            std::cout << "管道线程安全关闭完成" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "关闭管道线程时发生异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "关闭管道线程时发生未知异常" << std::endl;
        }
    }
    static void deletePipeLine()
    {

        // 通知所有线程退出
        for (size_t i = 0; i < m_pipe_processes.size(); ++i) {
            if(m_pipe_processes[i]){
                m_pipe_processes[i]->setExit(true);
            }

        }
        if (m_pipe2)
            m_pipe2->setExit(true);
        if (m_pipe1)
            m_pipe1->setExit(true);
        if (m_pipe0)
            m_pipe0->setExit(true);

        // 等待所有线程完成
        for (size_t i = 0; i < m_threads_processes.size(); ++i) {
            // 确保线程对象可等待
            if (m_threads_processes[i] && m_threads_processes[i]->joinable()) {
                m_threads_processes[i]->join();
                delete m_threads_processes[i];
            }
        }

        m_threads_processes.clear();

        if (m_pipe2) {
            // 确保不会在已删除的线程上调用 join
            if (m_t2 && m_t2->joinable())
                m_t2->join();

            // 删除线程对象
            delete m_t2;
            m_t2 = nullptr;
            m_pipe2 = nullptr;

        }

        // 终止主线程 m_t1
        if (m_t1&& m_t1->joinable()) {
            m_t1->join();
            delete m_t1;
            m_t1 = nullptr;
            m_pipe1 = nullptr;
        }

        // 终止线程 m_t0
        if (m_t0&& m_t0->joinable()) {
            m_t0->join();
            delete m_t0;
            m_t0 = nullptr;
            m_pipe0 = nullptr;
        }


        // 释放信号量资源
        m_dummySem.release();

        for (int i = 0; i < 6; i++) {
            m_processInSem[i].release();
            m_processOutSem[i].release();
        }
    }
    //暂停处理
    static void pausePipeLine(){
        for(auto pipe : m_pipe_processes){
            if(pipe){
                pipe->setPaused(true);
            }
        }
        if(m_pipe0) m_pipe0->setPaused(true);
        if(m_pipe1) m_pipe1->setPaused(true);
        if(m_pipe2) m_pipe2->setPaused(true);
    }


    //恢复处理
    static void resumePipeLine(){
        for(auto pipe : m_pipe_processes)
            if(pipe){
                pipe->setPaused(false);
            }
        if(m_pipe0) m_pipe0->setPaused(false);
        if(m_pipe1) m_pipe1->setPaused(false);
        if(m_pipe2) m_pipe2->setPaused(false);
    }

private:
    static QSemaphore m_processInSem[6];
    static QSemaphore m_processOutSem[6];
    static QSemaphore m_dummySem;
    static vector<thread *> m_threads_processes;
    static vector<AbstractPipe *>  m_pipe_processes;
    static AbstractPipe * m_pipe0;
    static AbstractPipe * m_pipe1;
    static AbstractPipe * m_pipe2;
    static AbstractPipe * m_pipe3;
    static thread * m_t0;
    static thread * m_t1;
    static thread * m_t2;
    static thread * m_t3;
    static Mat m_Images[6];
    static FrameImage m_FrameImages[6];
};

#endif // PIPLINE_H
