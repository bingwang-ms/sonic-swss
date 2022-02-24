#ifndef SWSS_UDFORCH_H
#define SWSS_UDFORCH_H

#include "saiudf.h"
#include "orch.h"
#include "switchorch.h"
#include "portsorch.h"

#include <vector>
#include <map>
#include <string>

using std::string;
using std::map;
using std::vector;

#define UDF_GROUP_TYPE      "TYPE"
#define UDF_GROUP_LEN       "LENGTH"

#define UDF_GROUP_TYPE_GENERIC "GENERIC"

#define UDF_MATCH_TYPE_L2       "L2"
#define UDF_MATCH_TYPE_L3       "L3"
#define UDF_MATCH_TYPE_GRE      "GRE"  

#define UDF_ATTR_MATCH          "MATCH"
#define UDF_ATTR_GROUP          "GROUP"
#define UDF_ATTR_BASE           "BASE"
#define UDF_ATTR_OFFSET         "OFFSET"

#define UDF_OFFSET_BASE_L2      "L2"
#define UDF_OFFSET_BASE_L3      "L3"
#define UDF_OFFSET_BASE_L4      "L4"


class UDFGroup {
    public:
        UDFGroup() = default;
        sai_attr_id_t oid;
        vector<sai_attribute_t> udf_group_attrs;
};

class UDFMatch {
    public:
        UDFMatch() = default;
        sai_attr_id_t oid;
        vector<sai_attribute_t> udf_match_attrs;
};

class UDFObject {
    public:
        UDFObject() = default;

        sai_attr_id_t oid;
        vector<sai_attribute_t> udf_object_attrs;
};


class UDFOrch : public Orch {
    public:
        UDFOrch(DBConnector* db, const vector<string> &tableNames);
        virtual ~UDFOrch();
        
        bool getUDFObjectByName(string name, UDFObject &udf_object) const;
        void removeUDFObjectByName(string name);
        
        bool getUDFGroupByName(string name, UDFGroup &udf_group) const;
        void removeUDFGroupByName(string name);

        bool getUDFMatchByName(string name, UDFMatch &udf_match) const;
        void removeUDFMatchByName(string name);

    protected:
        void doTask(Consumer &consumer);
        void doUDFGroupTask(Consumer &consumer);
        void doUDFMatchTask(Consumer &consumer);
        void doUDFObjectTask(Consumer &consumer);
    private:
        map<string, UDFGroup> m_UDFGroupTable;
        map<string, UDFMatch> m_UDFMatchTable;
        map<string, UDFObject> m_UDFObjectTable;
};

#endif
