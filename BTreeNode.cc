#include "BTreeNode.h"
#include <iostream>
#include <math.h>
#include <cstring>

using namespace std;

#define LEAF_ENTRY_SIZE         (sizeof(RecordId) + sizeof(int))
#define LEAF_CAPACITY           ((PageFile::PAGE_SIZE-sizeof(PageId))/LEAF_ENTRY_SIZE)
#define PAGE_ID_OFFSET          (PageFile::PAGE_SIZE - sizeof(PageId)-1)
#define NON_LEAF_ENTRY_SIZE     (sizeof(PageId) + sizeof(int))
#define NON_LEAF_CAPACITY       ((PageFile::PAGE_SIZE-sizeof(PageId))/NON_LEAF_ENTRY_SIZE)

BTLeafNode::BTLeafNode()
{
    std::fill(buffer, buffer + PageFile::PAGE_SIZE, 0);
}
/**
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf)
{
    return pf.read(pid,buffer);
}
    
/**
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf)
{
    return pf.write(pid,buffer);
}

/**
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount()
{
    int count = 0;
    int cursor = 0;
    int lastStartingIndex = (LEAF_CAPACITY-1)*LEAF_ENTRY_SIZE;
    while(cursor<=lastStartingIndex){
        if(isBlockEmpty(cursor)){
            break;
        }
        count ++;
        cursor+=LEAF_ENTRY_SIZE;
    }
    return count;
}

/**
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid)
{
    int keyCount = getKeyCount();
    if (keyCount == LEAF_CAPACITY){
        return RC_NODE_FULL;
    }
    int eid;
    // node alreay exists
    if(locate(key,eid)==0){
        return RC_INVALID_RID;
    }

    //insert at the end
    if(eid == keyCount){
        int insertionPoint = keyCount * LEAF_ENTRY_SIZE;
        memcpy(buffer+insertionPoint, &key, sizeof(int));
        memcpy(buffer+insertionPoint+sizeof(int), &rid, sizeof(RecordId));
    }
    //insert in the middle
    else{
        char temp[PageFile::PAGE_SIZE];
        std::fill(temp, temp + PageFile::PAGE_SIZE, 0);
        memcpy(temp, buffer, LEAF_ENTRY_SIZE*(keyCount));
        memcpy(buffer + (eid+1)*LEAF_ENTRY_SIZE, temp + (eid)*LEAF_ENTRY_SIZE, LEAF_ENTRY_SIZE*(keyCount-eid));
        memcpy(buffer + (eid)*LEAF_ENTRY_SIZE, &key, sizeof(int));
        memcpy(buffer + (eid)*LEAF_ENTRY_SIZE + sizeof(int), &rid, sizeof(RecordId));
    }

    return 0;
}

/**
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey)
{
    int keyCount = getKeyCount();
    int position;
    //key already exists
    if(locate(key,position) == 0 || keyCount ==0){
        return RC_INVALID_RID;
    }

    int firstHalf = keyCount/2;

    std::fill(sibling.buffer, sibling.buffer + PageFile::PAGE_SIZE, 0);
    memcpy(sibling.buffer, this->buffer + firstHalf*LEAF_ENTRY_SIZE, (keyCount-firstHalf)*LEAF_ENTRY_SIZE);
    std::fill(this->buffer + firstHalf*LEAF_ENTRY_SIZE, this->buffer + PageFile::PAGE_SIZE, 0);

    // should be inserted in the current node
    if(position<=firstHalf) {
        this->insert(key,rid);
    }else{
        sibling.insert(key,rid);
    }

    while ((this->getKeyCount() - sibling.getKeyCount())>1){
        int tempKey;
        RecordId tempId;
        this->readEntry(this->getKeyCount()-1,tempKey,tempId);
        std::fill(this->buffer + (getKeyCount()-1)*LEAF_ENTRY_SIZE, this->buffer + (this->getKeyCount())*LEAF_ENTRY_SIZE, 0);
        sibling.insert(tempKey, tempId);
    }

    while ((this->getKeyCount() - sibling.getKeyCount())<0){
        int tempKey;
        RecordId tempId;
        sibling.readEntry(0,tempKey,tempId);
        char temp[PageFile::PAGE_SIZE];
        memcpy(temp, sibling.buffer, PageFile::PAGE_SIZE);
        std::fill(sibling.buffer, sibling.buffer + PageFile::PAGE_SIZE, 0);
        memcpy(sibling.buffer, temp + LEAF_ENTRY_SIZE, PageFile::PAGE_SIZE-LEAF_ENTRY_SIZE);
        this->insert(tempKey,tempId);
    }

    int sKey;
    RecordId sRid;
    sibling.readEntry(0,sKey,sRid);
    siblingKey = sKey;
    return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid)
{
    for(int i=0;i<getKeyCount();i++){
        int key;
        RecordId recordId;
        readEntry(i,key,recordId);
        if (key == searchKey) {
            eid=i;
            return 0;
        }
        if(key > searchKey){
            eid = i;
            return RC_NO_SUCH_RECORD;
        }
    }
    eid = getKeyCount();
    return RC_NO_SUCH_RECORD;
}

/**
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid)
{
    if(eid>=getKeyCount() || eid<0){
        return RC_NO_SUCH_RECORD;
    }
    memcpy(&key, buffer + eid*LEAF_ENTRY_SIZE, sizeof(int));
    memcpy(&rid, buffer + eid*LEAF_ENTRY_SIZE + sizeof(int), sizeof(RecordId));
    return 0;
}

/**
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr()
{
    PageId pid;
    memcpy(&pid, buffer+PAGE_ID_OFFSET, sizeof(PageId));
    return pid;
}

/**
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid)
{
    memcpy(buffer+PAGE_ID_OFFSET,&pid,sizeof(PageId));
    return 0;
}

void BTLeafNode::print()
{
    cout<<"leaf capacity: " << LEAF_CAPACITY <<"\n";

    cout<<"key count: " << getKeyCount() <<"\n";
    char* temp=buffer;
    for(int i=0;i<getKeyCount();i++)
    {
        int key;
        memcpy(&key,temp,sizeof(int));
        cout<<"key: "<<key<<endl;
        temp+=LEAF_ENTRY_SIZE;
    }
}

bool BTLeafNode::isBlockEmpty(int startingIndex){
    char emptyBlock [LEAF_ENTRY_SIZE];
    memset (emptyBlock, 0, sizeof emptyBlock);
    return !memcmp(buffer + startingIndex, emptyBlock, LEAF_ENTRY_SIZE);
}



//////////////////////////////////////////////////////////////////////////////






BTNonLeafNode::BTNonLeafNode()
{
    std::fill(buffer, buffer + PageFile::PAGE_SIZE, 0);
}

/**
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{
    return pf.read(pid,buffer);
}
    
/**
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{
    return pf.write(pid,buffer);
}

/**
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{
    int count = 0;
    int cursor = sizeof(PageId);
    int lastStartingIndex = (NON_LEAF_CAPACITY-1)*NON_LEAF_ENTRY_SIZE + sizeof(PageId);
    while(cursor<=lastStartingIndex){
        if(isBlockEmpty(cursor)){
            break;
        }
        count ++;
        cursor+=NON_LEAF_ENTRY_SIZE;
    }
    return count;
}


/**
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{
    int keyCount = getKeyCount();
    if (keyCount == NON_LEAF_CAPACITY){
        return RC_NODE_FULL;
    }

    int eid;
    //duplicate key
    if(locate(key,eid)==0){
        return RC_INVALID_PID;
    }

    //insert at the end
    if(eid == keyCount){
        int insertionPoint = keyCount * NON_LEAF_ENTRY_SIZE + sizeof(PageId);
        memcpy(buffer+insertionPoint, &key, sizeof(int));
        memcpy(buffer+insertionPoint+sizeof(int), &pid, sizeof(PageId));
    }
        //insert in the middle
    else{
        char temp[PageFile::PAGE_SIZE];
        std::fill(temp, temp + PageFile::PAGE_SIZE, 0);
        memcpy(temp, buffer, NON_LEAF_ENTRY_SIZE*(keyCount) + sizeof(PageId));
        memcpy(buffer + (eid+1)*NON_LEAF_ENTRY_SIZE+sizeof(PageId), temp + (eid)*NON_LEAF_ENTRY_SIZE+ sizeof(PageId), NON_LEAF_ENTRY_SIZE*(keyCount-eid));
        memcpy(buffer + (eid)*NON_LEAF_ENTRY_SIZE+sizeof(PageId), &key, sizeof(int));
        memcpy(buffer + (eid)*NON_LEAF_ENTRY_SIZE+ sizeof(PageId)+sizeof(int), &pid, sizeof(PageId));
    }

    return 0;
}

/**
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{
    int keyCount = this->getKeyCount();
    int position;
    //duplicate key
    if(locate(key,position)==0){
        return RC_INVALID_PID;
    }
    int firstHalf;
    // the insertion key is the midkey
    if (position==ceil(keyCount/2) || position==floor(keyCount/2)){
        firstHalf = ceil(keyCount/2);
        memcpy(sibling.buffer,&pid, sizeof(PageId));
        memcpy(sibling.buffer+sizeof(PageId), this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId),
               (keyCount-firstHalf)*NON_LEAF_ENTRY_SIZE);
        std::fill(this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId), this->buffer + PageFile::PAGE_SIZE, 0);
        midKey = key;
    }
    //key should be inserted in first half
    else if(position<floor(keyCount/2)){
        firstHalf = floor(keyCount/2);
        memcpy(sibling.buffer+sizeof(PageId), this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId),
               (keyCount-firstHalf)*NON_LEAF_ENTRY_SIZE);
        std::fill(this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId), this->buffer + PageFile::PAGE_SIZE, 0);
        this->insert(key, pid);

        PageId midPid;
        this->readKeyPid(this->getKeyCount()-1, midKey, midPid);
        memcpy(sibling.buffer,&midPid, sizeof(PageId));
        int firstHalfKeyCount = this->getKeyCount();
        std::fill(this->buffer+(firstHalfKeyCount-1)*NON_LEAF_ENTRY_SIZE+ sizeof(PageId),
                  this->buffer+firstHalfKeyCount*NON_LEAF_ENTRY_SIZE+ sizeof(PageId), 0);
    }
    //key should be inserted in first half
    //position>ceil(keyCount/2)
    else{
        firstHalf = ceil(keyCount/2);
        memcpy(sibling.buffer+sizeof(PageId), this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId),
               (keyCount-firstHalf)*NON_LEAF_ENTRY_SIZE);
        std::fill(this->buffer + firstHalf*NON_LEAF_ENTRY_SIZE + sizeof(PageId), this->buffer + PageFile::PAGE_SIZE, 0);
        PageId midPid;
        sibling.insert(key, pid);
        sibling.readPidKey(0, midPid, midKey);
        char temp[PageFile::PAGE_SIZE];
        std::fill(temp, temp + sizeof(PageFile::PAGE_SIZE), 0);
        memcpy(temp, sibling.buffer + NON_LEAF_ENTRY_SIZE, PageFile::PAGE_SIZE - NON_LEAF_ENTRY_SIZE);
        memcpy(sibling.buffer,temp, PageFile::PAGE_SIZE);
    }


    return 0;
}

/**
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{
    int keyCount = getKeyCount();
    for(int i=0;i<keyCount;i++){
        PageId currentPid;
        int currentKey;
        readPidKey(i,currentPid,currentKey);
        if(searchKey<=currentKey){
            pid = currentPid;
        }
    }
    memcpy(&pid, buffer+keyCount*NON_LEAF_ENTRY_SIZE, sizeof(PageId));
    return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTNonLeafNode::locate(int searchKey, int& eid)
{
    for(int i=0;i<getKeyCount();i++){
        int key;
        PageId pageId;
        readPidKey(i,pageId,key);
        if (key == searchKey) {
            eid=i;
            return 0;
        }
        if(key > searchKey){
            eid = i;
            return RC_NO_SUCH_RECORD;
        }
    }
    eid = getKeyCount();
    return RC_NO_SUCH_RECORD;
}

RC BTNonLeafNode::readKeyPid(int eid, int& key, PageId& pid)
{
    if(eid>=getKeyCount() || eid<0){
        return RC_NO_SUCH_RECORD;
    }
    memcpy(&key, buffer + eid*NON_LEAF_ENTRY_SIZE+sizeof(PageId), sizeof(int));
    memcpy(&pid, buffer + (eid+1)*NON_LEAF_ENTRY_SIZE, sizeof(PageId));
    return 0;
}

RC BTNonLeafNode::readPidKey(int eid, PageId& pid, int& key)
{
    if(eid>=getKeyCount() || eid<0){
        return RC_NO_SUCH_RECORD;
    }
    memcpy(&pid, buffer + eid*NON_LEAF_ENTRY_SIZE, sizeof(PageId));
    memcpy(&key, buffer + eid*NON_LEAF_ENTRY_SIZE+sizeof(PageId), sizeof(int));
    return 0;
}
/**
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{
    std::fill(buffer, buffer + PageFile::PAGE_SIZE, 0);
    memcpy(buffer, &pid1, sizeof(PageId));
    memcpy(buffer + sizeof(PageId), &key, sizeof(int));
    memcpy(buffer + NON_LEAF_ENTRY_SIZE, &pid2, sizeof(PageId));
    return 0;
}

bool BTNonLeafNode::isBlockEmpty(int startingIndex){
    char emptyBlock [NON_LEAF_ENTRY_SIZE];
    memset (emptyBlock, 0, sizeof emptyBlock);
    return !memcmp(buffer + startingIndex, emptyBlock, NON_LEAF_ENTRY_SIZE);
}

void BTNonLeafNode::print()
{
    cout<<"key count: " << getKeyCount() <<"\n";

    int key;
    PageId pid;

    char* temp=buffer;
    readPidKey(0,pid,key);
    cout<<"pid: " <<pid <<endl;
    for(int i=0;i<getKeyCount();i++)
    {
        readKeyPid(i,key,pid);
        cout<<"key: "<<key<<endl;
        cout<<"pid: "<<pid<<endl;
        temp+=NON_LEAF_ENTRY_SIZE;
    }
    cout<<"----------------------"<<endl;
}