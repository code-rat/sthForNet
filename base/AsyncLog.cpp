#include "AsyncLog.h"

#include <ctime>
#include <time.h>
#include <sys/timeb.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <stdarg.h>
#include "../base/Platform.h"


#define MAX_LINE_LENGTH 256
#define DEFAULT_ROLL_SIZE 10*1024*1024

bool CAsyncLog::m_bTruncateLongLog=false;
FILE* CAsyncLog::m_hLogFile=NULL;
std::string CAsyncLog::m_strFileName="default";
std::string CAsyncLog::m_strFileNamePID="";
LOG_LEVEL CAsyncLog::m_nCurrentLevel=LOG_LEVEL_INFO;
int64_t CAsyncLog::m_nFileRollSize=DEFAULT_ROLL_SIZE;
int64_t CAsyncLog::m_nCurrentWrittenSize=0;
std::list<std::string> CAsyncLog::m_listLinesToWrite;
std::unique_ptr<std::thread> CAsyncLog::m_spWriteThread;
std::mutex CAsyncLog::m_mutexWrite;
std::condition_variable CAsyncLog::m_cvWrite;
bool CAsyncLog::m_bExit=false;
bool CAsyncLog::m_bRunning=false;

bool CAsyncLog::init(const char* pszLogFileName/*=nullptr*/,bool bTruncaateLongLine/*=false*/,int64_t nRollSize/*=10*1024*1024*/)
{
    m_bTruncateLongLog=bTruncaateLongLine;
    m_nFileRollSize=nRollSize;

    if(pszLogFileName==nullptr || pszLogFileName[0]==0){
        m_strFileName.clear();
    }
    else{
        m_strFileName=pszLogFileName;
    }

    //获取进程ID
    char szPID[8];
    snprintf(szPID,sizeof(szPID),"%05d",(int)::getpid());

    m_strFileNamePID=szPID;
    //创建文件夹
    m_spWriteThread.reset(new std::thread(writeThreadProc));
    return true;
}

void CAsyncLog::uninit()
{
    m_bExit=true;
    m_cvWrite.notify_one();
    
    if(m_spWriteThread->joinable()){
        m_spWriteThread->join();
    }

    if(m_hLogFile!=nullptr)
    {
        fclose(m_hLogFile);
        m_hLogFile=nullptr;
    }
}

void CAsyncLog::setLevel(LOG_LEVEL nLevel)
{
    if(nLevel<LOG_LEVEL_TRACE || nLevel>LOG_LEVEL_FATAL){
        return;
    }
    m_nCurrentLevel=nLevel;
}

bool CAsyncLog::isRunning()
{
    return m_bRunning;
}

bool CAsyncLog::output(long nLevel,const char* pszFmt,...)
{
    if(nLevel!=LOG_LEVEL_CRITICAL){
        if(m_nCurrentLevel>nLevel){
            return false;
        }
    }

    std::string strLine;
    makeLinePrefix(nLevel,strLine);

    //log正文
    std::string strLogMsg;

    //计算下不定参数的长度  以便于分配空间 pszFmt是格式化的字符串 va_start第二个参数是   后面...是配置的一系列参数
    va_list ap;
    va_start(ap,pszFmt);
    int nLogMsgLength=vsnprintf(NULL,0,pszFmt,ap);
    va_end(ap);

    if((int)strLogMsg.capacity()<nLogMsgLength+1)
    {
        strLogMsg.resize(nLogMsgLength+1);
    }

    va_list aq;
    va_start(aq,pszFmt);
    vsnprintf((char*)strLogMsg.data(),strLogMsg.capacity(),pszFmt,aq);
    va_end(aq);

    //string 内容正确但是length不对  恢复以下length？？？？？？？   
    std::string strMsgFormal;
    strMsgFormal.append(strLogMsg.c_str(),nLogMsgLength);

    //如果日志开启截断   长日志只取前MAX_LINE_LENGTH个字符
    if(m_bTruncateLongLog){
        strMsgFormal.substr(0,MAX_LINE_LENGTH);
    }

    strLine+=strMsgFormal;

    //不是输出到控制台才会在每一行末尾加一个换行符
    if(!m_strFileName.empty())
    {
        strLine+="\n";
    }

    if(nLevel!=LOG_LEVEL_FATAL){
        std::lock_guard<std::mutex> lock_guard(m_mutexWrite);
        m_listLinesToWrite.push_back(strLine);
        m_cvWrite.notify_one();
    }
    else{
        //为了让FATAL级别的日志能立即crash程序，采取同步写日志的方法
        std::cout<<strLine<<std::endl;

        if(!m_strFileName.empty()){
            if(m_hLogFile==nullptr){
                //新建文件
                char szNow[64];
                time_t now = time(NULL);
                tm time;
                localtime_r(&now, &time);

                strftime(szNow, sizeof(szNow), "%Y%m%d%H%M%S", &time);

                std::string strNewFileName(m_strFileName);
                strNewFileName += ".";
                strNewFileName += szNow;
                strNewFileName += ".";
                strNewFileName += m_strFileNamePID;
                strNewFileName += ".log";
                if (!createNewFile(strNewFileName.c_str()))
                    return false;
            }
            writeToFile(strLine);
        }
        //让程序主动crash掉
        crash();
    }
    return true;
}

bool CAsyncLog::output(long nLevel,const char* pszFileName,int nLineNo,const char* pszFmt,...)
{
    if(nLevel!=LOG_LEVEL_CRITICAL){
        if(m_nCurrentLevel>nLevel){
            return false;
        }
    }

    std::string strLine;
    makeLinePrefix(nLevel,strLine);

    //函数签名
    char szName[512]={0};
    snprintf(szName,sizeof(szName),"[%s:%d]",pszFileName,nLineNo);
    strLine+=szName;

    //log正文
    std::string strLogMsg;

    //计算下不定参数的长度  以便于分配空间 pszFmt是格式化的字符串 va_start第二个参数是   后面...是配置的一系列参数
    va_list ap;
    va_start(ap,pszFmt);
    int nLogMsgLength=vsnprintf(NULL,0,pszFmt,ap);
    va_end(ap);

    if((int)strLogMsg.capacity()<nLogMsgLength+1)
    {
        strLogMsg.resize(nLogMsgLength+1);
    }

    va_list aq;
    va_start(aq,pszFmt);
    vsnprintf((char*)strLogMsg.data(),strLogMsg.capacity(),pszFmt,aq);
    va_end(aq);

    //string 内容正确但是length不对  恢复以下length？？？？？？？   
    std::string strMsgFormal;
    strMsgFormal.append(strLogMsg.c_str(),nLogMsgLength);

    //如果日志开启截断   长日志只取前MAX_LINE_LENGTH个字符
    if(m_bTruncateLongLog){
        strMsgFormal.substr(0,MAX_LINE_LENGTH);
    }

    strLine+=strMsgFormal;

    //不是输出到控制台才会在每一行末尾加一个换行符
    if(!m_strFileName.empty())
    {
        strLine+="\n";
    }

    if(nLevel!=LOG_LEVEL_FATAL){
        std::lock_guard<std::mutex> lock_guard(m_mutexWrite);
        m_listLinesToWrite.push_back(strLine);
        m_cvWrite.notify_one();
    }
    else{
        //为了让FATAL级别的日志能立即crash程序，采取同步写日志的方法
        std::cout<<strLine<<std::endl;

        if(!m_strFileName.empty()){
            if(m_hLogFile==nullptr){
                //新建文件
                char szNow[64];
                time_t now = time(NULL);
                tm time;
                localtime_r(&now, &time);

                strftime(szNow, sizeof(szNow), "%Y%m%d%H%M%S", &time);

                std::string strNewFileName(m_strFileName);
                strNewFileName += ".";
                strNewFileName += szNow;
                strNewFileName += ".";
                strNewFileName += m_strFileNamePID;
                strNewFileName += ".log";
                if (!createNewFile(strNewFileName.c_str()))
                    return false;
            }
            writeToFile(strLine);
        }
        //让程序主动crash掉
        crash();
    }
    return true;
}

bool CAsyncLog::outputBinary(unsigned char* buffer,size_t size)
{
    std::ostringstream os;

    static const size_t PRINTSIZE = 512;
    char szbuf[PRINTSIZE * 3 + 8];

    size_t lsize = 0;
    size_t lprintbufsize = 0;
    int index = 0;
    os << "address[" << (long)buffer << "] size[" << size << "] \n";
    while (true)
    {
        memset(szbuf, 0, sizeof(szbuf));
        if (size > lsize)
        {
            lprintbufsize = (size - lsize);
            lprintbufsize = lprintbufsize > PRINTSIZE ? PRINTSIZE : lprintbufsize;
            formLog(index, szbuf, sizeof(szbuf), buffer + lsize, lprintbufsize);
            size_t len = strlen(szbuf);

            //if (stream().buffer().avail() < static_cast<int>(len))
            //{
            //    stream() << szbuf + (len - stream().buffer().avail());
            //    break;
            //}
            //else
            //{
            os << szbuf;
            //}
            lsize += lprintbufsize;
        }
        else
        {
            break;
        }
    }

    std::lock_guard<std::mutex> lock_guard(m_mutexWrite);
    m_listLinesToWrite.push_back(os.str());
    m_cvWrite.notify_one();

    return true;
}

const char* CAsyncLog::ullto4Str(int n)
{
    static char buf[64+1];
    memset(buf,0,sizeof(buf));
    sprintf(buf,"%06u",n);
    return buf;
}

char* CAsyncLog::fromLog(int index,char* szbuf,size_t size_buf,unsigned char* buffer,size_t size)
{

}

//补全日志输出的前几个部分  级别时间   线程ID
void CAsyncLog::makeLinePrefix(long nLevel,std::string &strPrefix)
{
    //级别
    strPrefix="[INFO]";
    switch (nLevel)
    {
    case LOG_LEVEL_TRACE:
        strPrefix="[TRACE]";
        break;
    case LOG_LEVEL_DEBUG:
        strPrefix="[DEBUG]";
        break;
    case LOG_LEVEL_WARNING:
        strPrefix="[WARN]";
        break;
    case LOG_LEVEL_ERROR:
        strPrefix="[ERROR]";
        break;
    case LOG_LEVEL_SYSERROR:
        strPrefix="[SYSE]";
        break;
    case LOG_LEVEL_FATAL:
        strPrefix="[FATAL]";
        break;
    case LOG_LEVEL_CRITICAL:
        strPrefix="[CRITICAL]";
        break;
    default:
        break;
    }

    //时间
    char szTime[64]={0};
    getTime(szTime,sizeof(szTime));

    strPrefix+="[";
    strPrefix+=szTime;
    strPrefix+="]";

    //当前线程信息
    char szThreadID[32]={0};
    std::ostringstream osThreadID;
    osThreadID<<std::this_thread::get_id();
    snprintf(szThreadID,sizeof(szThreadID),"[%s]",osThreadID.str().c_str());
    strPrefix+=szThreadID;
}

void CAsyncLog::getTime(char* pszTime,int nTimeStrLength)
{
    struct timeb tp;
    ftime(&tp);

    time_t now=tp.time;
    tm time;
    //原来获取系统时间 linux平台函数 windows localtime_s
    localtime_r(&now,&time);

    snprintf(pszTime,nTimeStrLength,"[%04d-%02d-%02d %02d:%02d:%02d:%03d]",time.tm_year+1900,time.tm_mon+1,time.tm_mday,time.tm_hour,time.tm_min,time.tm_sec);
}

bool CAsyncLog::createNewFile(const char*pszLogFileName)
{
    if(m_hLogFile!=nullptr)
    {
        fclose(m_hLogFile);
    }   

    //新建文件
    m_hLogFile=fopen(pszLogFileName,"w+");
    return m_hLogFile!=nullptr;
}

bool CAsyncLog::writeToFile(const std::string& data)
{
    //为了防止长文件一次性写不完    放在循环里面分批写
    std::string strLocal(data);
    int ret=0;
    while (true)
    {
        ret=fwrite(strLocal.c_str(),1,strLocal.length(),m_hLogFile);
        if(ret<=0){
            return false;
        }
        else if(ret<=(int)strLocal.length()){
            strLocal.erase(0,ret);
        }

        if(strLocal.empty()){
            break;
        }
    }

    fflush(m_hLogFile);
    return true;
}

void CAsyncLog::crash()
{
    char *p=nullptr;
    *p=0;
}

void CAsyncLog::writeThreadProc()
{
    m_bRunning=true;

    while (true)
    {
        if(!m_strFileName.empty()){
            if(m_hLogFile==nullptr|| m_nCurrentWrittenSize>=m_nFileRollSize)
            {
                //重置m_nCurrentWrittenSize大小
                m_nCurrentWrittenSize=0;

                //第一次或者文件大小rollsize，均新建文件
                char szNow[64];
                time_t now=time(NULL);
                tm time;
                localtime_r(&now,&time);

                strftime(szNow,sizeof(szNow),"%Y%m%d%H%M%s",&time);

                std::string strNewFileName(m_strFileName);
                strNewFileName+=".";
                strNewFileName+=szNow;
                strNewFileName+=".";
                strNewFileName+=m_strFileNamePID;
                strNewFileName+=".log";
                if(!createNewFile(strNewFileName.c_str())){
                    return;
                }
            }
        }

        std::string strLine;
        {
            std::unique_lock<std::mutex> guard(m_mutexWrite);
            while (m_listLinesToWrite.empty())
            {
                if(m_bExit){
                    return;
                }
                m_cvWrite.wait(guard);
            }
            strLine=m_listLinesToWrite.front();
            m_listLinesToWrite.pop_front();
        }        
        std::cout<<strLine<<std::endl;

        if(!m_strFileName.empty()){
            if(!writeToFile(strLine))
                return;
            m_nCurrentWrittenSize+=strLine.length();
        }
    }
    m_bRunning=false;
}

