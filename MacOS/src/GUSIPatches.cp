/*
 * GUSIPatches.cp
 */

#include "lshprefix.h"
#include "lsh_context.h"

#include <GUSIInternal.h>
#include <GUSIBasics.h>
#include <GUSIContext.h>
#include <GUSIConfig.h>
#include <GUSIDiag.h>
#include <GUSISocket.h>
#include <GUSIPThread.h>
#include <GUSIFactory.h>
#include <GUSIDevice.h>
#include <GUSIDescriptor.h>
#include <GUSIOTNetDB.h>

#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef __CONSOLE__
#include <console.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

void ssh2_init();

int open(const char * path, int mode, ...);
int dup(int s);
int socket(int domain, int type, int protocol);
int close(int s);

void ssh2_doevent(long sleepTime);
void add_one_file(struct lshcontext *context, int fd);
void remove_one_file(struct lshcontext *context, int fd);

#ifdef __cplusplus
}
#endif


/*
 * default GUSIProcess::Yield has 20 ticks to remain in same state
 * we use 0.
 */

/* GUSI 2.1.3
void GUSIProcess::Yield(GUSIYieldMode wait)
{
	ProcessSerialNumber	front;
	Boolean				same;

	if (wait == kGUSIBlock) {
		fWillSleep = true;
		if (fReadyThreads > 1 || fDontSleep)
			wait = kGUSIYield;
	}
	if (wait == kGUSIYield && LMGetTicks() - fResumeTicks < 0) {
		return;
	}
	if (fClosing)
		fClosing->CheckClose();
	if (gGUSISpinHook) {
		gGUSISpinHook(wait == kGUSIBlock);
	} else {
		GUSI_SMESSAGE("Suspend\n");
		GUSIHandleNextEvent(wait == kGUSIBlock ? 600 : 0);
		GUSI_SMESSAGE("Resume\n");
	} 
		
	fWillSleep 		= false;
	fDontSleep 		= false;
	fResumeTicks 	= LMGetTicks();
}
*/

/* GUSI 2.1.5 */
void GUSIProcess::Yield(GUSIYieldMode wait)
{
	if (wait == kGUSIBlock) {
		fWillSleep = true;
		if (fReadyThreads > 1 || fDontSleep) {
			GUSI_SMESSAGE("Don't Sleep\n");
			wait = kGUSIYield;
		}
	}
	if (fExistingThreads < 2) // Single threaded process skips sleep only once
		fDontSleep = false;
	if (wait == kGUSIYield && LMGetTicks() - fResumeTicks < 1) {
		fWillSleep 		= false;
		return;
	}
	if (gGUSISpinHook) {
		gGUSISpinHook(wait == kGUSIBlock);
	} else {
		GUSI_SMESSAGE("Suspend\n");
		GUSIHandleNextEvent(wait == kGUSIBlock ? 60 : 0);
		GUSI_SMESSAGE("Resume\n");
	} 
	if (fExistingThreads < 2) 		// Single threaded process skips sleep only once
		fDontSleep = false;
	fWillSleep 		= false;
	fResumeTicks 	= LMGetTicks();
	if (fClosing)
		fClosing->CheckClose();
}

/*
 * default GUSIContext::Yield has 12 ticks to remain in same state
 * we use 0.
 */

bool GUSIContext::Yield(GUSIYieldMode wait)
{
	if (wait == kGUSIYield && LMGetTicks() - sCurrentContext->fEntryTicks < 0)
		return false;	

	bool			mainThread	= sCurrentContext->fThreadID == kApplicationThreadID;
	bool 			block		= wait == kGUSIBlock && !mainThread;
	GUSIProcess	*	process 	= GUSIProcess::Instance();
	bool 			interrupt 	= false;
	
	do {
		if (mainThread)
			process->Yield(wait);
	
		if (interrupt = Raise())
			goto done;
				
		if (sHasThreading) {
			if (block)
				SetThreadState(kCurrentThreadID, kStoppedThreadState, kNoThreadID);
			else
				YieldToAnyThread();
		}
	} while (wait == kGUSIBlock && !sCurrentContext->fWakeup);
done:
	sCurrentContext->fWakeup = false;
	
	return interrupt;
}

/*
 * The fSvc field of the GUSIOTNetDB instance is no longer valid after
 * an interface switch in the TCP/IP control panel.
 * Let's clear it upon kOTProviderWillClose message.
 */

// <Asynchronous notifier function for [[GUSIOTNetDB]]>=                   
inline uint32_t CompleteMask(OTEventCode code)	
{ 	
	return 1 << (code & 0x1F); 
}

pascal void GUSIOTNetDBNotify(
	GUSIOTNetDB * netdb, OTEventCode code, OTResult result, void *cookie)
{
	GUSI_MESSAGE(("GUSIOTNetDBNotify %08x %d\n", code, result));
	GUSIContext *	context = netdb->fCreationContext;
	
	switch (code & 0x7F000000L) {
	case 0:
		netdb->fEvent |= code;
		result = 0;
		break;
	case kPRIVATEEVENT:
	case kCOMPLETEEVENT:
		if (!(code & 0x00FFFFE0))
			netdb->fCompletion |= CompleteMask(code);
		switch (code) {
		case T_OPENCOMPLETE:
			netdb->fSvc = static_cast<InetSvcRef>(cookie);
			break;
		case T_DNRSTRINGTOADDRCOMPLETE:
		case T_DNRADDRTONAMECOMPLETE:
			context = static_cast<GUSIContext **>(cookie)[-1];
			break;
		}
		break;
	default:
		if (code != kOTProviderWillClose)
			result = 0;
		else {
			OTCloseProvider(netdb->fSvc);
			netdb->fSvc = NULL;
			netdb->fCreationContext = NULL;
		}
		break;
	}
	if (result)
		netdb->fAsyncError = result;
	context->Wakeup();
}





/*
 * we need to track open()/dup()/close()/socket() calls to close files/sockets
 * upon abort/exit
 */

/*
 * open.
 */

int	open(const char * path, int mode, ...)
{
	GUSIErrorSaver			saveError;
	GUSIDeviceRegistry *	factory	= GUSIDeviceRegistry::Instance();
	GUSIDescriptorTable *	table	= GUSIDescriptorTable::Instance();
	GUSISocket * 			sock;
	int						fd;
	
	if (sock = factory->open(path, mode)) {
		if ((fd = table->InstallSocket(sock)) > -1) {
			lshcontext *context = (lshcontext *)pthread_getspecific(ssh2threadkey);
			if ( context ) {
				add_one_file(context, fd);
			}
			return fd;
		}
		sock->close();
	}
	if (!errno)
		return GUSISetPosixError(ENOMEM);
	else
		return -1;
}

/*
 * dup.
 */

int dup(int s)
{
	GUSIDescriptorTable		*table	= GUSIDescriptorTable::Instance();
	GUSISocket				*sock	= GUSIDescriptorTable::LookupSocket(s);
	int						fd;
	lshcontext				*context = (lshcontext *)pthread_getspecific(ssh2threadkey);

	if (!sock)
		return -1;

	fd = table->InstallSocket(sock);
	if ( context ) {
		add_one_file(context, fd);
	}
	return fd;
}

/*
 * dup2.
 */

int dup2(int s, int s1)
{
	GUSIDescriptorTable		*table	= GUSIDescriptorTable::Instance();
	GUSISocket				*sock	= GUSIDescriptorTable::LookupSocket(s);
	int						fd;
	lshcontext				*context = (lshcontext *)pthread_getspecific(ssh2threadkey);

	if (!sock)
		return -1;

	table->RemoveSocket(s1);
	fd = table->InstallSocket(sock, s1);
	if ( context && s1 != fd ) {
		remove_one_file(context, s1);
		add_one_file(context, fd);
	}
	return fd;
}

/*
 * socket.
 */

int socket(int domain, int type, int protocol)
{
	GUSIErrorSaver			saveError;
	GUSISocketFactory *		factory	= GUSISocketDomainRegistry::Instance();
	GUSIDescriptorTable *	table	= GUSIDescriptorTable::Instance();
	GUSISocket * 			sock;
	int						fd;
	
	if (sock = factory->socket(domain, type, protocol)) {
		if ((fd = table->InstallSocket(sock)) > -1) {
			lshcontext *context = (lshcontext *)pthread_getspecific(ssh2threadkey);
			if ( context ) {
				add_one_file(context, fd);
			}
			return fd;
		}
		sock->close();
	}
	if (!errno)
		return GUSISetPosixError(ENOMEM);
	else
		return -1;
}

/*
 * close.
 */

int close(int s)
{
	if ( s > STDERR_FILENO ) {
		GUSIDescriptorTable *	table	= GUSIDescriptorTable::Instance();
		lshcontext *context = (lshcontext *)pthread_getspecific(ssh2threadkey);
		if ( context ) {
			remove_one_file(context, s);
		}
		return table->RemoveSocket(s);
	} else {
		/* we don't close stdin/stdout/stderr */
		return 0;
	}
}

#pragma mark -

/*
 * ssh2_init
 */

void ssh2_init()
{
	static Boolean		sGUSISetup = false;

	if ( !sGUSISetup ) {
		GUSIContext::Setup(true);
		/*GUSISetupConsole();*/
		sGUSISetup = true;
	}
}

