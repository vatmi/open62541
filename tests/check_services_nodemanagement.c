/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "check.h"
#include "server/ua_nodestore.h"
#include "server/ua_services.h"
#include "ua_client.h"
#include "ua_types.h"
#include "ua_config_standard.h"
#include "server/ua_server_internal.h"

#ifdef UA_ENABLE_MULTITHREADING
#include <pthread.h>
#include <urcu.h>
#endif

static UA_Server *server = NULL;
static UA_ServerConfig *config = NULL;

static void setup(void) {
    config = UA_ServerConfig_new_default();
    server = UA_Server_new(config);
}

static void teardown(void) {
    UA_Server_delete(server);
    UA_ServerConfig_delete(config);
}

static UA_StatusCode
instantiationMethod(UA_NodeId newNodeId, UA_NodeId templateId, void *handle ) {
  *((UA_Int32 *) handle) += 1;
  return UA_STATUSCODE_GOOD;
}
START_TEST(AddVariableNode) {
    /* add a variable node to the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en_US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer");
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_StatusCode res = UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId, parentReferenceNodeId,
                                                  myIntegerName, UA_NODEID_NULL, attr, NULL, NULL);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, res);
} END_TEST

START_TEST(AddComplexTypeWithInheritance) {
  /* add a variable node to the address space */
  UA_ObjectAttributes attr;
  UA_ObjectAttributes_init(&attr);
  attr.description = UA_LOCALIZEDTEXT("en_US","fakeServerStruct");
  attr.displayName = UA_LOCALIZEDTEXT("en_US","fakeServerStruct");
  
  UA_NodeId myObjectNodeId = UA_NODEID_STRING(1, "the.fake.Server.Struct");
  UA_QualifiedName myObjectName = UA_QUALIFIEDNAME(1, "the.fake.Server.Struct");
  UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
  UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
  UA_Int32 handleCalled = 0;
  UA_InstantiationCallback iCallback = {.method=instantiationMethod, .handle = (void *) &handleCalled};
    
  UA_StatusCode res = UA_Server_addObjectNode(server, myObjectNodeId, parentNodeId, parentReferenceNodeId,
                                              myObjectName, UA_NODEID_NUMERIC(0, 2004),
                                              attr, &iCallback, NULL);
  ck_assert_int_eq(UA_STATUSCODE_GOOD, res);
  ck_assert_int_gt(handleCalled, 0); // Should be 58, but may depend on NS0 XML detail
} END_TEST

START_TEST(AddNodeTwiceGivesError) {
    /* add a variable node to the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en_US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer");
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_StatusCode res = UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId, parentReferenceNodeId,
                                                  myIntegerName, UA_NODEID_NULL, attr, NULL, NULL);
    ck_assert_int_eq(UA_STATUSCODE_GOOD, res);
    res = UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId, parentReferenceNodeId,
                                    myIntegerName, UA_NODEID_NULL, attr, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_BADNODEIDEXISTS);
} END_TEST

static UA_Boolean constructorCalled = false;

static void * objectConstructor(const UA_NodeId instance) {
    constructorCalled = true;
    return NULL;
}

START_TEST(AddObjectWithConstructor) {
    /* Add an object type */
    UA_NodeId objecttypeid = UA_NODEID_NUMERIC(0, 13371337);
    UA_ObjectTypeAttributes attr;
    UA_ObjectTypeAttributes_init(&attr);
    attr.displayName = UA_LOCALIZEDTEXT("en_US","my objecttype");
    UA_StatusCode res = UA_Server_addObjectTypeNode(server, objecttypeid,
                                                    UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                                    UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                                    UA_QUALIFIEDNAME(0, "myobjecttype"), attr, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Add a constructor to the object type */
    UA_ObjectLifecycleManagement olm = {objectConstructor, NULL};
    res = UA_Server_setObjectTypeNode_lifecycleManagement(server, objecttypeid, olm);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Add an object of the type */
    UA_ObjectAttributes attr2;
    UA_ObjectAttributes_init(&attr2);
    attr2.displayName = UA_LOCALIZEDTEXT("en_US","my object");
    res = UA_Server_addObjectNode(server, UA_NODEID_NULL, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), UA_QUALIFIEDNAME(0, ""),
                                  objecttypeid, attr2, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Verify that the constructor was called */
    ck_assert_int_eq(constructorCalled, true);
} END_TEST

static UA_Boolean destructorCalled = false;

static void objectDestructor(const UA_NodeId instance, void *handle) {
    destructorCalled = true;
}

START_TEST(DeleteObjectWithDestructor) {
    /* Add an object type */
    UA_NodeId objecttypeid = UA_NODEID_NUMERIC(0, 13371337);
    UA_ObjectTypeAttributes attr;
    UA_ObjectTypeAttributes_init(&attr);
    attr.displayName = UA_LOCALIZEDTEXT("en_US","my objecttype");
    UA_StatusCode res = UA_Server_addObjectTypeNode(server, objecttypeid,
                                                    UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                                    UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                                    UA_QUALIFIEDNAME(0, "myobjecttype"), attr, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Add a constructor to the object type */
    UA_ObjectLifecycleManagement olm = {NULL, objectDestructor};
    res = UA_Server_setObjectTypeNode_lifecycleManagement(server, objecttypeid, olm);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Add an object of the type */
    UA_NodeId objectid = UA_NODEID_NUMERIC(0, 23372337);
    UA_ObjectAttributes attr2;
    UA_ObjectAttributes_init(&attr2);
    attr2.displayName = UA_LOCALIZEDTEXT("en_US","my object");
    res = UA_Server_addObjectNode(server, objectid, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), UA_QUALIFIEDNAME(0, ""),
                                  objecttypeid, attr2, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Delete the object */
    UA_Server_deleteNode(server, objectid, true);

    /* Verify that the destructor was called */
    ck_assert_int_eq(destructorCalled, true);
} END_TEST

START_TEST(DeleteObjectAndReferences) {
    /* Add an object of the type */
    UA_ObjectAttributes attr;
    UA_ObjectAttributes_init(&attr);
    attr.displayName = UA_LOCALIZEDTEXT("en_US","my object");
    UA_NodeId objectid = UA_NODEID_NUMERIC(0, 23372337);
    UA_StatusCode res;
    res = UA_Server_addObjectNode(server, objectid, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), UA_QUALIFIEDNAME(0, ""),
                                  UA_NODEID_NULL, attr, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Verify that we have a reference to the node from the objects folder */
    UA_BrowseDescription bd;
    UA_BrowseDescription_init(&bd);
    bd.nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    bd.referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
    bd.browseDirection = UA_BROWSEDIRECTION_FORWARD;
    
    UA_BrowseResult br = UA_Server_browse(server, 0, &bd);
    ck_assert_int_eq(br.statusCode, UA_STATUSCODE_GOOD);
    size_t refCount = 0;
    for(size_t i = 0; i < br.referencesSize; ++i) {
        if(UA_NodeId_equal(&br.references[i].nodeId.nodeId, &objectid))
            refCount++;
    }
    ck_assert_int_eq(refCount, 1);
    UA_BrowseResult_deleteMembers(&br);

    /* Delete the object */
    UA_Server_deleteNode(server, objectid, true);

    /* Browse again, this time we expect that no reference is found */
    br = UA_Server_browse(server, 0, &bd);
    ck_assert_int_eq(br.statusCode, UA_STATUSCODE_GOOD);
    refCount = 0;
    for(size_t i = 0; i < br.referencesSize; ++i) {
        if(UA_NodeId_equal(&br.references[i].nodeId.nodeId, &objectid))
            refCount++;
    }
    ck_assert_int_eq(refCount, 0);
    UA_BrowseResult_deleteMembers(&br);

    /* Add an object the second time */
    UA_ObjectAttributes_init(&attr);
    attr.displayName = UA_LOCALIZEDTEXT("en_US","my object");
    objectid = UA_NODEID_NUMERIC(0, 23372337);
    res = UA_Server_addObjectNode(server, objectid, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), UA_QUALIFIEDNAME(0, ""),
                                  UA_NODEID_NULL, attr, NULL, NULL);
    ck_assert_int_eq(res, UA_STATUSCODE_GOOD);

    /* Browse again, this time we expect that a single reference to the node is found */
    refCount = 0;
    br = UA_Server_browse(server, 0, &bd);
    ck_assert_int_eq(br.statusCode, UA_STATUSCODE_GOOD);
    for(size_t i = 0; i < br.referencesSize; ++i) {
        if(UA_NodeId_equal(&br.references[i].nodeId.nodeId, &objectid))
            refCount++;
    }
    ck_assert_int_eq(refCount, 1);
    UA_BrowseResult_deleteMembers(&br);
} END_TEST


/* Example taken from tutorial_server_object.c */
START_TEST(InstantiateObjectType) {
    /* Define the object type */
    UA_NodeId pumpTypeId = {1, UA_NODEIDTYPE_NUMERIC, {1001}};

    UA_StatusCode retval;

    /* Define the object type for "Device" */
    UA_NodeId deviceTypeId; /* get the nodeid assigned by the server */
    UA_ObjectTypeAttributes dtAttr;
    UA_ObjectTypeAttributes_init(&dtAttr);
    dtAttr.displayName = UA_LOCALIZEDTEXT("en_US", "DeviceType");
    retval = UA_Server_addObjectTypeNode(server, UA_NODEID_NULL,
                                         UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                                         UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                         UA_QUALIFIEDNAME(1, "DeviceType"), dtAttr,
                                         NULL, &deviceTypeId);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    UA_VariableAttributes mnAttr;
    UA_VariableAttributes_init(&mnAttr);
    mnAttr.displayName = UA_LOCALIZEDTEXT("en_US", "ManufacturerName");
    UA_NodeId manufacturerNameId;
    retval = UA_Server_addVariableNode(server, UA_NODEID_NULL, deviceTypeId,
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "ManufacturerName"),
                                       UA_NODEID_NULL, mnAttr, NULL, &manufacturerNameId);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
    /* Make the manufacturer name mandatory */
    retval = UA_Server_addReference(server, manufacturerNameId,
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_HASMODELLINGRULE),
                                    UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_MODELLINGRULE_MANDATORY), true);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    UA_VariableAttributes modelAttr;
    UA_VariableAttributes_init(&modelAttr);
    modelAttr.displayName = UA_LOCALIZEDTEXT("en_US", "ModelName");
    retval = UA_Server_addVariableNode(server, UA_NODEID_NULL, deviceTypeId,
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "ModelName"),
                                       UA_NODEID_NULL, modelAttr, NULL, NULL);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    /* Define the object type for "Pump" */
    UA_ObjectTypeAttributes ptAttr;
    UA_ObjectTypeAttributes_init(&ptAttr);
    ptAttr.displayName = UA_LOCALIZEDTEXT("en_US", "PumpType");
    retval = UA_Server_addObjectTypeNode(server, pumpTypeId,
                                         deviceTypeId, UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                         UA_QUALIFIEDNAME(1, "PumpType"), ptAttr,
                                         NULL, NULL);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    UA_VariableAttributes statusAttr;
    UA_VariableAttributes_init(&statusAttr);
    statusAttr.displayName = UA_LOCALIZEDTEXT("en_US", "Status");
    statusAttr.valueRank = -1;
    UA_NodeId statusId;
    retval = UA_Server_addVariableNode(server, UA_NODEID_NULL, pumpTypeId,
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "Status"),
                                       UA_NODEID_NULL, statusAttr, NULL, &statusId);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    /* Make the status variable mandatory */
    retval = UA_Server_addReference(server, statusId,
                                    UA_NODEID_NUMERIC(0, UA_NS0ID_HASMODELLINGRULE),
                                    UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_MODELLINGRULE_MANDATORY), true);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    UA_VariableAttributes rpmAttr;
    UA_VariableAttributes_init(&rpmAttr);
    rpmAttr.displayName = UA_LOCALIZEDTEXT("en_US", "MotorRPM");
    rpmAttr.valueRank = -1;
    retval = UA_Server_addVariableNode(server, UA_NODEID_NULL, pumpTypeId,
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                       UA_QUALIFIEDNAME(1, "MotorRPMs"),
                                       UA_NODEID_NULL, rpmAttr, NULL, NULL);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);

    /* Instantiate the variable */
    UA_ObjectAttributes oAttr;
    UA_ObjectAttributes_init(&oAttr);
    oAttr.displayName = UA_LOCALIZEDTEXT("en_US", "MyPump");
    retval = UA_Server_addObjectNode(server, UA_NODEID_NULL,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "MyPump"),
                                     pumpTypeId, /* this refers to the object type
                                                    identifier */
                                     oAttr, NULL, NULL);
    ck_assert_int_eq(retval, UA_STATUSCODE_GOOD);
} END_TEST

int main(void) {
    Suite *s = suite_create("services_nodemanagement");

    TCase *tc_addnodes = tcase_create("addnodes");
    tcase_add_checked_fixture(tc_addnodes, setup, teardown);
    tcase_add_test(tc_addnodes, AddVariableNode);
    tcase_add_test(tc_addnodes, AddComplexTypeWithInheritance);
    tcase_add_test(tc_addnodes, AddNodeTwiceGivesError);
    tcase_add_test(tc_addnodes, AddObjectWithConstructor);
    tcase_add_test(tc_addnodes, InstantiateObjectType);

    TCase *tc_deletenodes = tcase_create("deletenodes");
    tcase_add_checked_fixture(tc_deletenodes, setup, teardown);
    tcase_add_test(tc_deletenodes, DeleteObjectWithDestructor);
    tcase_add_test(tc_deletenodes, DeleteObjectAndReferences);

    suite_add_tcase(s, tc_addnodes);
    suite_add_tcase(s, tc_deletenodes);

    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
