#include "pipline.h"

QSemaphore pipline::m_processInSem[6];
QSemaphore pipline::m_processOutSem[6];
QSemaphore pipline::m_dummySem;
AbstractPipe * pipline::m_pipe0;
AbstractPipe * pipline::m_pipe1;
AbstractPipe * pipline::m_pipe2;
AbstractPipe * pipline::m_pipe3;
thread * pipline::m_t0;
thread * pipline::m_t1;
thread * pipline::m_t2;
thread * pipline::m_t3;
vector<AbstractPipe *>  pipline::m_pipe_processes;
vector<thread *> pipline::m_threads_processes;
Mat pipline::m_Images[6];
FrameImage pipline::m_FrameImages[6];

pipline::pipline()
{

}
