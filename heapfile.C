/*
* Brandon Frauenfeld, 907 708 3880, frauenfeld
* Kath Oku, 907 869 6235, kmoku
* Khai Bui, 907 264 9824, kmbui2
*
* This file is a part of the HeapFile Manager for the database management system - Minirel -  
* specified in Project Stage 4 description
* 
* heapfile.C implements the heapfile routines, and the HeapFile, HeapFileScan, InsertFileScan classes as declared in heapfile.h.
*/

#include "heapfile.h"
#include "error.h"

/* Routine to create a heapfile
*
* Creates an almost empty heapfile. Initializes header page of heapfile.
* Initializes everything properly for a header page as well.
*
* @param const string fileName - the fileName of the new file created
* @Return status, OK if no errors, or not OK depending on function errors
*/
const Status createHeapFile(const string fileName)
{
    File* 		file;
    Status 		status;
    FileHdrPage*	hdrPage;
    int			hdrPageNo;
    int			newPageNo;
    Page*		newPage;

    // try to open the file. This should return an error
    status = db.openFile(fileName, file);
    if (status != OK)
    {
		// file doesn't exist. First create it and allocate
		// an empty header page and data page.
		
		// create a db level file by calling db->createfile()
        status = db.createFile(fileName);
        if (status != OK) {
            return status;
        }
        db.openFile(fileName, file);
        // Then, allocate an empty page by invoking bm->allocPage() appropriately
        status = bufMgr->allocPage(file, newPageNo, newPage);
        if (status != OK) {
            return status;
        }
        // allocPage() will return a pointer to an empty page in the buffer pool along with the page number of the page.
        // Take the Page* pointer returned from allocPage() and cast it to a FileHdrPage*
        hdrPage = (FileHdrPage*) newPage;
        // Using this pointer initialize the values in the header page.
        // as this is header page, it is the first page.
        hdrPageNo = newPageNo;
        // additionally, set file name.
        strcpy(hdrPage->fileName, fileName.c_str()); // need .c_str because fileName is a const string 
        // Then make a second call to bm->allocPage(). This page will be the first data page of the file.
        status = bufMgr->allocPage(file, newPageNo, newPage);
        if (status != OK) {
            return status;
        }
        // Using the Page* pointer returned, invoke its init() method to initialize the page contents.
        newPage->init(newPageNo);
        // Finally, store the page number of the data page in firstPage and lastPage attributes of the FileHdrPage.
        hdrPage->firstPage = newPageNo;
        hdrPage->lastPage = newPageNo;
	// init other parts of hdrPage (to be safe)
        hdrPage->recCnt = 0;
        hdrPage->pageCnt = 1;
        // When you have done all this unpin both pages and mark them as dirty.
        status = bufMgr->unPinPage(file, hdrPageNo, true); // "true" marks dirty bit
        if (status != OK) {
            return status;
        }
        status = bufMgr->unPinPage(file, newPageNo, true); // ditto
        if (status != OK) {
            return status;
        }
        db.closeFile(file); // close or else delete segfaults
		// all done, return OK
		return OK;
    }
    db.closeFile(file);
    return (FILEEXISTS);
}
// routine to destroy a heapfile
const Status destroyHeapFile(const string fileName)
{
	return (db.destroyFile(fileName));
}

/*
*   HeapFile Constructor - opens the underlying file
*
*   @params fileName - name of file (string)
*   @params resturnStatus - returns OK if successful
*
*/
HeapFile::HeapFile(const string & fileName, Status& returnStatus)
{
    Status 	status;
    Page*	pagePtr;

    cout << "opening file " << fileName << endl;

    // open the file and read in the header page and the first data page
    if ((status = db.openFile(fileName, filePtr)) == OK)
    {
		
		//do not forget to save the File* returned in the filePtr data member
        File* file = filePtr;
        int pageNo = -1;
        //get header page
        status = filePtr->getFirstPage(pageNo);
        if (status != OK) {
            cerr << "getFirstPage failed \n";
            returnStatus = status;
            return;
        } 
        //read and pin header page for file in buffer pool
        status = bufMgr->readPage(file, pageNo, pagePtr);
        if(status != OK) {
            cerr << "readPage failed \n";
            returnStatus = status;
            return;
        }

        //initializing the private data members headerPage, headerPageNo, and hdrDirtyFlag
        headerPage = (FileHdrPage*) pagePtr;
        headerPageNo = pageNo;
        hdrDirtyFlag = false;

        //read and pin the first page of the file
        int firstPageNo = headerPage->firstPage;
        status = bufMgr->readPage(filePtr, firstPageNo, curPage);
        if(status != OK) {
            cerr << "readPage failed \n";
            returnStatus = status;
            return;
        }
        //init curPage, curPageNo, and curDirtyFlag
        //curPage = pagePtr;
        curPageNo = firstPageNo;
        curDirtyFlag = false;
        //set curRec to NULLRID
        curRec = NULLRID;
        returnStatus = OK;
        return;
    }
    else
    {
    	cerr << "open of heap file failed\n";
		returnStatus = status;
		return;
    }
}

// the destructor closes the file
HeapFile::~HeapFile()
{
    Status status;
    cout << "invoking heapfile destructor on file " << headerPage->fileName << endl;

    // see if there is a pinned data page. If so, unpin it 
    if (curPage != NULL)
    {
    	status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
		curPage = NULL;
		curPageNo = 0;
		curDirtyFlag = false;
		if (status != OK) cerr << "error in unpin of date page\n";
    }
	
	 // unpin the header page
    status = bufMgr->unPinPage(filePtr, headerPageNo, hdrDirtyFlag);
    if (status != OK) cerr << "error in unpin of header page\n";
	
	// status = bufMgr->flushFile(filePtr);  // make sure all pages of the file are flushed to disk
	// if (status != OK) cerr << "error in flushFile call\n";
	// before close the file
	status = db.closeFile(filePtr);
    if (status != OK)
    {
		cerr << "error in closefile call\n";
		Error e;
		e.print (status);
    }
}

// Return number of records in heap file

const int HeapFile::getRecCnt() const
{
  return headerPage->recCnt;
}


/*
*   retrieve an arbitrary record from a file.
*   if record is not on the currently pinned page, the current page
*   is unpinned and the required page is read into the buffer pool
*   and pinned.  returns a pointer to the record via the rec parameter
*
*   @params rid - record ID
*   @params red - reference to record
*
*   @return status if successful, returns record via rec param
*/
const Status HeapFile::getRecord(const RID & rid, Record & rec)
{
    Status status;

    // cout<< "getRecord. record (" << rid.pageNo << "." << rid.slotNo << ")" << endl;
    
    //If the desired record is on the currently pinned page
    if(curPage && curPageNo == rid.pageNo) {
        //curPage->getRecord(rid, rec) to get the record
        status=curPage->getRecord(rid, rec);
    } else {
        //otherwise, unpin currently pinned page (assuming a page is pinned)
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        if (status != OK) {
            return status;
        }
        curPageNo = rid.pageNo;
        curRec = rid;
        curDirtyFlag = false;
        //use pageNo field of RID to read the page into the buffer pool
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if(status != OK) {
            return status;
        }
        //return pointer to the record via rec
        status = curPage->getRecord(curRec,rec);
    }

    return status;
   
   
   
   
   
   
}

HeapFileScan::HeapFileScan(const string & name,
			   Status & status) : HeapFile(name, status)
{
    filter = NULL;
}

const Status HeapFileScan::startScan(const int offset_,
				     const int length_,
				     const Datatype type_, 
				     const char* filter_,
				     const Operator op_)
{
    if (!filter_) {                        // no filtering requested
        filter = NULL;
        return OK;
    }
    
    if ((offset_ < 0 || length_ < 1) ||
        (type_ != STRING && type_ != INTEGER && type_ != FLOAT) ||
        (type_ == INTEGER && length_ != sizeof(int)
         || type_ == FLOAT && length_ != sizeof(float)) ||
        (op_ != LT && op_ != LTE && op_ != EQ && op_ != GTE && op_ != GT && op_ != NE))
    {
        return BADSCANPARM;
    }

    offset = offset_;
    length = length_;
    type = type_;
    filter = filter_;
    op = op_;

    return OK;
}


const Status HeapFileScan::endScan()
{
    Status status;
    // generally must unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        curPage = NULL;
        curPageNo = 0;
		curDirtyFlag = false;
        return status;
    }
    return OK;
}

HeapFileScan::~HeapFileScan()
{
    endScan();
}

const Status HeapFileScan::markScan()
{
    // make a snapshot of the state of the scan
    markedPageNo = curPageNo;
    markedRec = curRec;
    return OK;
}

const Status HeapFileScan::resetScan()
{
    Status status;
    if (markedPageNo != curPageNo) 
    {
		if (curPage != NULL)
		{
			status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
			if (status != OK) return status;
		}
		// restore curPageNo and curRec values
		curPageNo = markedPageNo;
		curRec = markedRec;
		// then read the page
		status = bufMgr->readPage(filePtr, curPageNo, curPage);
		if (status != OK) return status;
		curDirtyFlag = false; // it will be clean
    }
    else curRec = markedRec;
    return OK;
}

/*
* Fetch the RID of the next record that satisfies the given predicate
*
* @param outRid The RID of the next record
*
* @return status OK if successful, otherwise relay the error status
*/
const Status HeapFileScan::scanNext(RID& outRid)
{
    Status 	status = OK;
    RID		nextRid;
    RID		tmpRid;
    int 	nextPageNo;
    Record      rec;

    // Check for the EOF or uninitialized page
    if (curPageNo == -1)
        return FILEEOF;

    if (curPage == NULL) {
        status = bufMgr->readPage(filePtr, curPageNo, curPage);  // Also pin the new page
        if (status != OK) return status;
    }
    tmpRid = curRec;

    while (true) {
        // Get the next RID, either from the current or the next page
        if (curPage->nextRecord(tmpRid, nextRid) != OK) {
            do {
                curPage->getNextPage(nextPageNo);
                if (nextPageNo == -1 || curPageNo == headerPage->lastPage || curPageNo == -1)
                    return FILEEOF;
                status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag); // Unpin current page
                if (status != OK) return status;
                status = bufMgr->readPage(filePtr, nextPageNo, curPage);  // Also pin the new page
                if (status != OK) return status;
                curPageNo = nextPageNo;
            } while (curPage->firstRecord(nextRid) != OK);
        }
        
        // Fetch record
        status = curPage->getRecord(nextRid, rec); 
        if (status != OK) break;
        
        // Check predicate
        if (matchRec(rec)) {
            curRec = outRid = nextRid;
            break;
        } 
        // Continue searching
        tmpRid = nextRid;
    }	
	
	return status;
}


// returns pointer to the current record.  page is left pinned
// and the scan logic is required to unpin the page 

const Status HeapFileScan::getRecord(Record & rec)
{
    return curPage->getRecord(curRec, rec);
}

// delete record from file. 
const Status HeapFileScan::deleteRecord()
{
    Status status;

    // delete the "current" record from the page
    status = curPage->deleteRecord(curRec);
    curDirtyFlag = true;

    // reduce count of number of records in the file
    headerPage->recCnt--;
    hdrDirtyFlag = true; 
    return status;
}


// mark current page of scan dirty
const Status HeapFileScan::markDirty()
{
    curDirtyFlag = true;
    return OK;
}

const bool HeapFileScan::matchRec(const Record & rec) const
{
    // no filtering requested
    if (!filter) return true;

    // see if offset + length is beyond end of record
    // maybe this should be an error???
    if ((offset + length -1 ) >= rec.length)
	return false;

    float diff = 0;                       // < 0 if attr < fltr
    switch(type) {

    case INTEGER:
        int iattr, ifltr;                 // word-alignment problem possible
        memcpy(&iattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ifltr,
               filter,
               length);
        diff = iattr - ifltr;
        break;

    case FLOAT:
        float fattr, ffltr;               // word-alignment problem possible
        memcpy(&fattr,
               (char *)rec.data + offset,
               length);
        memcpy(&ffltr,
               filter,
               length);
        diff = fattr - ffltr;
        break;

    case STRING:
        diff = strncmp((char *)rec.data + offset,
                       filter,
                       length);
        break;
    }

    switch(op) {
    case LT:  if (diff < 0.0) return true; break;
    case LTE: if (diff <= 0.0) return true; break;
    case EQ:  if (diff == 0.0) return true; break;
    case GTE: if (diff >= 0.0) return true; break;
    case GT:  if (diff > 0.0) return true; break;
    case NE:  if (diff != 0.0) return true; break;
    }

    return false;
}

InsertFileScan::InsertFileScan(const string & name,
                               Status & status) : HeapFile(name, status)
{
    // Heapfile constructor will read the header page and the first
    // data page of the file into the buffer pool
    // If the first data page of the file is not the last data page of the file
    // unpin the current page and read the last page
    if ((curPage != NULL) && (curPageNo != headerPage->lastPage))
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        if (status != OK) cerr << "error in unpin of data page\n";
            curPageNo = headerPage->lastPage;
            status = bufMgr->readPage(filePtr, curPageNo, curPage);
        if (status != OK) cerr << "error in readPage \n";
        curDirtyFlag = false;
    }
}

InsertFileScan::~InsertFileScan()
{
    Status status;
    // unpin last page of the scan
    if (curPage != NULL)
    {
        status = bufMgr->unPinPage(filePtr, curPageNo, true);
        curPage = NULL;
        curPageNo = 0;
        if (status != OK) cerr << "error in unpin of data page\n";
    }
}

/*
* Insert a record into the file and return its' RID
*
* @param rec The Record object that will be inserted
* @param outRid The corresponding RID of the inserted record
*
* @return status OK if successful, otherwise relay the error status
*/
const Status InsertFileScan::insertRecord(const Record & rec, RID& outRid)
{
    Page*	newPage;
    int		newPageNo;
    Status	status, unpinstatus;
    RID		rid;

    // check for very large records
    if ((unsigned int) rec.length > PAGESIZE-DPFIXED)
    {
        // will never fit on a page, so don't even bother looking
        return INVALIDRECLEN;
    }

    // Read the last page if curPage isn't initialized
    if (curPage == NULL) {
        curPageNo = headerPage->lastPage;
        status = bufMgr->readPage(filePtr, curPageNo, curPage);
    }

    // Insert record in the current, or the next page
    if (curPage->insertRecord(rec, rid) != OK) {
        status = bufMgr->allocPage(filePtr, newPageNo, newPage);
        if (status != OK) {
            return status; 
        }        
        curPage->setNextPage(newPageNo);
        // Unpin current page
        unpinstatus = bufMgr->unPinPage(filePtr, curPageNo, curDirtyFlag);
        
        // Set new page as curPage
        newPage->init(newPageNo);
        curPage = newPage;
        curPageNo = newPageNo;
        curDirtyFlag = false;

        // Update header on the new page        
        hdrDirtyFlag = true; 
        ++(headerPage->pageCnt);
        headerPage->lastPage = curPageNo;

        // Insert record in the new page
        status = curPage->insertRecord(rec, rid);
        if (status != OK) {
            return status; 
        }
    } 
    curDirtyFlag = true;
    outRid = rid;

    // Update header on the new record
    hdrDirtyFlag = true; 
    ++(headerPage->recCnt);
    return OK;
}


