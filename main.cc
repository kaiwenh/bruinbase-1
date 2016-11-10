/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */

#include "Bruinbase.h"
#include "SqlEngine.h"
#include <cstdio>
#include "BTreeNode.h"
#include "RecordFile.h"
#include <iostream>
#include <fstream>

using namespace std;

int main()
{
//////***********  Leaf node tests ************************////
//    RecordId recordId = RecordId();
//
//    BTLeafNode n =  BTLeafNode();
//    BTLeafNode s = BTLeafNode();
//
//    for(int i=0;i<13;i=i+2){
//        int k = i;
//        n.insert(k,recordId);
//    }
//    n.print();
//    int sKey;
//    n.insertAndSplit(1,recordId,s,sKey);
//
//    cout << "sibling head key: " << sKey <<"\n";
//    n.print();
//    cout << "-----------\n";
//    s.print();
//

//////***********  Non Leaf node tests ************************////

    BTNonLeafNode a = BTNonLeafNode();
    a.initializeRoot(10000,0,10000);
    a.print();
    for(int i=0;i<=13;i=i+2){
        cout<<"-->inserting key: " <<i<<endl;
        a.insert(i,i);
        a.print();
    }

    a.print();
    BTNonLeafNode b = BTNonLeafNode();
    int midKey;
    a.insertAndSplit(1,1,b,midKey);
    a.print();
    b.print();
    cout<<"midkey is: " << midKey <<"\n";
  return 0;
}


//
///**
// * Copyright (C) 2008 by The Regents of the University of California
// * Redistribution of this file is permitted under the terms of the GNU
// * Public License (GPL).
// *
// * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
// * @date 3/24/2008
// */
//
//#include "Bruinbase.h"
//#include "SqlEngine.h"
//#include <cstdio>
//
//int main()
//{
//    // run the SQL engine taking user commands from standard input (console).
//    SqlEngine::run(stdin);
//
//    return 0;
//}
