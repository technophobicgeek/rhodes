#pragma once

#ifdef __cplusplus

#include "logging/RhoLog.h"
#include "common/RhoThread.h"
#include "common/RhoMutexLock.h"
#include "common/IRhoClassFactory.h"

namespace rho {
namespace common {

class CThreadQueue : public common::CRhoThread
{
protected:
    static const unsigned int QUEUE_POLL_INTERVAL_SECONDS  = 300;
    static const unsigned int QUEUE_POLL_INTERVAL_INFINITE  = (unsigned int)(-1);
    static const unsigned int QUEUE_STARTUP_INTERVAL_SECONDS  = 10;

public:
    DEFINE_BASELOGCLASS

    struct IQueueCommand
    {
        virtual ~IQueueCommand(){};
        virtual boolean equals(const IQueueCommand& cmd) = 0;
        virtual String toString() = 0;
        virtual void cancel(){}
    };

private:

	int           m_nPollInterval;
   	common::CMutex m_mxStackCommands;
	LinkedListPtr<IQueueCommand*> m_stackCommands;
    IQueueCommand* m_pCurCmd;

    boolean m_bNoThreaded;
public:
    CThreadQueue();

    ~CThreadQueue(void);

    virtual void addQueueCommand(IQueueCommand* pCmd);
    virtual void addQueueCommandToFront(IQueueCommand* pCmd);
	virtual void run();
    virtual void stop(unsigned int nTimeoutToKill);

	void setPollInterval(int nInterval);
    int  getPollInterval()const{ return m_nPollInterval;}

    boolean isNoThreadedMode(){ return m_bNoThreaded; }
    void setNonThreadedMode(boolean b){m_bNoThreaded = b;}

    void cancelCurrentCommand();
    common::CMutex& getCommandLock(){ return m_mxStackCommands; }
    IQueueCommand* getCurCommand(){ return m_pCurCmd; }
    LinkedListPtr<IQueueCommand*>& getCommands(){ return m_stackCommands; }

protected:
    virtual int getLastPollInterval(){ return 0;}
    virtual void processCommand(IQueueCommand* pCmd) = 0;
    virtual boolean isSkipDuplicateCmd() { return false; }
    virtual void onTimeout(){}

    boolean isAlreadyExist(IQueueCommand *pCmd);

    void processCommands();
    void processCommandBase(IQueueCommand* pCmd);

    void addQueueCommandInt(IQueueCommand* pCmd);
    void addQueueCommandToFrontInt(IQueueCommand* pCmd);

    boolean isNoCommands();
};

}
}
#endif //__cplusplus

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
	
#ifdef __cplusplus
};
#endif //__cplusplus


