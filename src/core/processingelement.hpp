#ifndef _NANOS_PROCESSING_ELEMENT
#define _NANOS_PROCESSING_ELEMENT

#include "queue.hpp"
#include "workdescriptor.hpp"

namespace nanos {

//TODO: some things should be moved from PE to here...
//TODO: define BaseThread interface
class BaseThread {
private:
  int id;
  bool started;
  bool mustStop;
  ProcessingElement *pe;

//disable copy and assigment
  BaseThread(const BaseThread &);
  const BaseThread operator= (const BaseThread &);

  virtual void run_dependent () = 0;
  
public:
  // constructor
  BaseThread (ProcessingElement *creator=0) : started(false), mustStop(false), pe(creator) {}
  // destructor
  virtual ~BaseThread() {}

  virtual void start () = 0;
  void run();
  void stop() { mustStop = true; }
  virtual void join() = 0;

  bool isRunning () const { return started && !mustStop; }

//TODO:
// void pause();
// void resume();
  int getId() { return id; }
};

// forward definitions
class SchedulingGroup;
class SchedulingData;

class ProcessingElement {
private:
	int id;
	const Architecture *architecture;
	SchedulingGroup *schedGroup;
	SchedulingData  *schedData;


protected:
	virtual WorkDescriptor & getWorkerWD () const = 0;
	WD *    currentWD;
	BaseThread *workerThread;

public:
	// constructors
	ProcessingElement(int newId,const Architecture *arch,SchedulingGroup *sg=0);
	// TODO: copy constructor
	ProcessingElement(const ProcessingElement &pe);
	// TODO: assignment operations
	const ProcessingElement & operator= (const ProcessingElement &pe);
	// destructor
	virtual ~ProcessingElement() {}

	/* get/put methods */
	int getId() const { return id; }
	const Architecture * getArchitecture () const { return architecture; }
	SchedulingGroup * getSchedulingGroup () const { return schedGroup; }
	SchedulingData * getSchedulingData () const { return schedData; }
	void setSchedulingGroup (SchedulingGroup *sg, SchedulingData *sd)
	    { schedGroup = sg; schedData = sd; }

	virtual BaseThread & startThread (WorkDescriptor &wd) = 0;
	
	virtual void processWork () = 0;
	// TODO: if not defined, it should be a fatal exception
	virtual BaseThread & associateThisThread () = 0;
	// initializes thread-private data. Must be invoked from the thread code
	void associate();

	void setCurrentWD (WD *current) { currentWD = current; }
	WD * getCurrentWD () const { return currentWD; }

	void startWorker();
	void stopAll();

	//TODO: move to BaseThread
	bool isRunning() { return workerThread->isRunning(); }

	virtual void switchTo(WD *work) = 0;
	virtual void exitTo(WD *work) = 0;
};

typedef class ProcessingElement PE;

// Each thread should be able to locate who is its PE at any moment
extern __thread PE *myPE;

};

#endif
