#include "udforch.h"
#include "converter.h"
#include "return_code.h"

extern sai_switch_api_t* sai_switch_api;
extern sai_object_id_t   gSwitchId;
extern PortsOrch*        gPortsOrch;
extern sai_udf_api_t *sai_udf_api;

using std::string;
using std::map;

using namespace swss;

map<string, sai_udf_group_attr_t> udf_group_attr_table = 
{
    {UDF_GROUP_TYPE_GENERIC,    SAI_UDF_GROUP_ATTR_TYPE},
    {UDF_GROUP_LEN,             SAI_UDF_GROUP_ATTR_LENGTH}
};

map<string, sai_udf_group_type_t> udf_group_type_table = 
{
    {UDF_GROUP_TYPE_GENERIC,    SAI_UDF_GROUP_TYPE_GENERIC}
};

map<string, sai_udf_match_attr_t> udf_match_type_table = 
{
    {UDF_MATCH_TYPE_L2,         SAI_UDF_MATCH_ATTR_L2_TYPE},
    {UDF_MATCH_TYPE_L3,         SAI_UDF_MATCH_ATTR_L3_TYPE},
    {UDF_MATCH_TYPE_GRE,        SAI_UDF_MATCH_ATTR_GRE_TYPE}
};

map<string, sai_udf_attr_t> udf_object_attr_table = 
{
    {UDF_ATTR_BASE,             SAI_UDF_ATTR_BASE},
    {UDF_ATTR_GROUP,            SAI_UDF_ATTR_GROUP_ID},
    {UDF_ATTR_MATCH,            SAI_UDF_ATTR_MATCH_ID},
    {UDF_ATTR_OFFSET,           SAI_UDF_ATTR_OFFSET}
};

map<string, sai_udf_base_t> udf_offset_base_table = 
{
    {UDF_OFFSET_BASE_L2,        SAI_UDF_BASE_L2},
    {UDF_OFFSET_BASE_L3,        SAI_UDF_BASE_L3},
    {UDF_OFFSET_BASE_L4,        SAI_UDF_BASE_L4}    
};

bool UDFOrch::getUDFGroupByName(string name, UDFGroup &udf_group) const
{
    auto iter = m_UDFGroupTable.find(name);
    if (iter == m_UDFGroupTable.end())
    {
        return false;
    }
    udf_group = iter->second;
    return true;
}

void UDFOrch::removeUDFGroupByName(string name)
{
    auto iter = m_UDFGroupTable.find(name);
    if (iter != m_UDFGroupTable.end())
    {
        m_UDFGroupTable.erase(iter);
    }
}

bool UDFOrch::getUDFMatchByName(string name, UDFMatch &udf_match) const
{
    auto iter = m_UDFMatchTable.find(name);
    if (iter == m_UDFMatchTable.end())
    {
        return false;
    }
    udf_match = iter->second;
    return true;
}

void UDFOrch::removeUDFMatchByName(string name)
{
    auto iter = m_UDFMatchTable.find(name);
    if (iter != m_UDFMatchTable.end()) {
        m_UDFMatchTable.erase(iter);
    }
}

bool UDFOrch::getUDFObjectByName(string name, UDFObject &udf_object) const
{
    auto iter = m_UDFObjectTable.find(name);
    if (iter == m_UDFObjectTable.end())
    {
        return false;
    }
    udf_object = iter->second;
    return true;
}

void UDFOrch::removeUDFObjectByName(string name)
{
    auto iter = m_UDFObjectTable.find(name);
    if (iter != m_UDFObjectTable.end())
    {
        m_UDFObjectTable.erase(iter);
    }
}

UDFOrch::UDFOrch(DBConnector* db, const vector<string> &tableNames):
    Orch(db, tableNames)
{

}

UDFOrch::~UDFOrch()
{
    
}

void UDFOrch::doTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();
    
    if (!gPortsOrch->allPortsReady())
    {
        return;
    }
    string table_name = consumer.getTableName();

    if (CFG_UDF_GROUP_TABLE_NAME == table_name)
    {
        doUDFGroupTask(consumer);
    }
    else if (CFG_UDF_MATCH_TABLE_NAME == table_name)
    {
        doUDFMatchTask(consumer);
    }
    else if (CFG_UDF_OBJECT_TABLE_NAME == table_name)
    {
        doUDFObjectTask(consumer);
    }
    else 
    {
        SWSS_LOG_ERROR("Invalid table %s", table_name.c_str());
    }
}

void UDFOrch::doUDFGroupTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;
        string group_name = kfvKey(t);
        string op = kfvOp(t);

        if (SET_COMMAND == op)
        {
            UDFGroup udf_group;
            if (getUDFGroupByName(group_name, udf_group))
            {
                // Duplicated group name found, ignore the request
                SWSS_LOG_ERROR("Duplicated UDF group name found: %s", group_name.c_str());
            }
            else
            {
                bool valid = true;
                // Scan all attributes
                for (auto itp : kfvFieldsValues(t))
                {
                    string attr_name = to_upper(fvField(itp));
                    string attr_value = fvValue(itp);
                    SWSS_LOG_DEBUG("TABLE ATTRIBUTE: %s : %s", attr_name.c_str(), attr_value.c_str());

                    auto iter = udf_group_attr_table.find(attr_name);
                    if (iter == udf_group_attr_table.end())
                    {
                        SWSS_LOG_WARN("Unsupported UDF group attribute found: %s for UDF group %s", attr_name.c_str(), group_name.c_str());
                        valid = false;
                        continue;
                    }
                    sai_attribute_t attr;
                    attr.id = iter->second;
                    if (UDF_GROUP_TYPE == attr_name)
                    {
                        auto type_iter = udf_group_type_table.find(attr_value);
                        if (type_iter == udf_group_type_table.end())
                        {
                            SWSS_LOG_ERROR("Unsupported UDF type: %s", attr_value.c_str());
                            valid = false;
                            break;
                        }
                        attr.value.s32 = type_iter->second;
                        udf_group.udf_group_attrs.emplace_back(attr);
                    }
                    else if (UDF_GROUP_LEN == attr_name)
                    {
                        attr.value.s16 = to_uint<uint16_t>(attr_value);
                        udf_group.udf_group_attrs.emplace_back(attr);
                    }
                }
                if (valid)
                {
                    sai_object_id_t udf_group_oid;

                    CHECK_ERROR_AND_LOG_AND_RETURN(sai_udf_api->create_udf_group(&udf_group_oid, gSwitchId,
                                                                            (uint32_t)udf_group.udf_group_attrs.size(),
                                                                            udf_group.udf_group_attrs.data()),
                                            "Failed to create UDF group " << group_name
                                                                            << " from SAI call sai_udf_api->create_udf_group");
                    udf_group.oid = udf_group_oid;
                    m_UDFGroupTable.emplace(group_name, udf_group);
                    SWSS_LOG_INFO("Suceeded to create UDF group %s with object ID %s ", group_name.c_str(),
                                    sai_serialize_object_id(udf_group_oid).c_str());
                }
            }
        }
        else if (DEL_COMMAND == op)
        {
            // TODO: Add ref count
            UDFGroup udf_group;
            if (!getUDFGroupByName(group_name, udf_group))
            {
                // Not found
                
                SWSS_LOG_ERROR("UDF group not found: %s", group_name.c_str());
                continue;
            }
 
            CHECK_ERROR_AND_LOG_AND_RETURN(sai_udf_api->remove_udf_group(udf_group.oid),
                                   "Failed to remove UDF group with id " << udf_group_id);
            removeUDFGroupByName(group_name);
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());
        }
        it = consumer.m_toSync.erase(it);
    }
}

void UDFOrch::doUDFMatchTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;
        string udf_match_name = kfvKey(t);
        string op = kfvOp(t);

        if (SET_COMMAND == op)
        {
            UDFMatch udf_match;
            if (getUDFMatchByName(udf_match_name, udf_match))
            {
                // Duplicated udf match name found, ignore the request
                SWSS_LOG_ERROR("Duplicated UDF match name found: %s", udf_match_name.c_str());
            }
            else
            {
                bool valid = true;
                // Scan all attributes
                for (auto itp : kfvFieldsValues(t))
                {
                    string attr_name = to_upper(fvField(itp));
                    string attr_value = fvValue(itp);
                    SWSS_LOG_DEBUG("TABLE ATTRIBUTE: %s : %s", attr_name.c_str(), attr_value.c_str());

                    auto iter = udf_match_type_table.find(attr_name);
                    if (iter == udf_match_type_table.end()) 
                    {
                        SWSS_LOG_ERROR("Unsupported UDF match type found: %s for %s", attr_name.c_str(), udf_match_name.c_str());
                        continue;
                    }
                    sai_attribute_t attr;
                    attr.id = iter->second;
                    attr.value.s16 = to_uint<uint16_t>(attr_value);
                    udf_match.udf_match_attrs.emplace_back(attr);
                }
                if (udf_match.udf_match_attrs.empty())
                {
                    SWSS_LOG_ERROR("Missing match attributes for UDF match %s", udf_match_name.c_str());
                }
                else
                {
                    CHECK_ERROR_AND_LOG_AND_RETURN(sai_udf_api->create_udf_match(&udf_match.oid, gSwitchId, 0, udf_match.udf_match_attrs.data()),
                                                    "Failed to create UDF match %s from SAI call sai_udf_api->create_udf_match" << udf_match_name);
                    m_UDFMatchTable.emplace(udf_match_name, udf_match);
                    SWSS_LOG_INFO("Suceeded to create UDF match %s with object ID %s ", udf_match_name.c_str(),
                                    sai_serialize_object_id(udf_match_oid).c_str());
                }
            }
        }
        else if (DEL_COMMAND == op)
        {
            // TODO: Add ref count
            UDFMatch udf_match;
            if (!getUDFMatchByName(udf_match_name, udf_match))
            {
                // Not found
                SWSS_LOG_ERROR("UDF match not found: %s", udf_match_name.c_str());
                continue;
            }
 
            CHECK_ERROR_AND_LOG_AND_RETURN(sai_udf_api->remove_udf_match(udf_match.oid),
                                   "Failed to remove UDF match with name " << udf_match_name);
            removeUDFMatchByName(udf_match_name);
        }
        else
        {
            SWSS_LOG_ERROR("Unknown operation type %s", op.c_str());

        }
        it = consumer.m_toSync.erase(it);
    }
}

void UDFOrch::doUDFObjectTask(Consumer &consumer)
{
    SWSS_LOG_ENTER();

    auto it = consumer.m_toSync.begin();
    while (it != consumer.m_toSync.end())
    {
        KeyOpFieldsValuesTuple t = it->second;
        string udf_object_name = kfvKey(t);
        string op = kfvOp(t);

        if (SET_COMMAND == op)
        {
            UDFObject udf_object;
            if (getUDFObjectByName(udf_object_name, udf_object))
            {
                SWSS_LOG_ERROR("Duplicated UDF object name found %s", udf_object_name.c_str());
            }
            else
            {
                bool valid = true;
                UDFObject udf_object;
                for (auto itp : kfvFieldsValues(t))
                {
                    string attr_name = to_upper(fvField(itp));
                    string attr_value = fvValue(itp);
                    SWSS_LOG_DEBUG("TABLE ATTRIBUTE: %s : %s", attr_name.c_str(), attr_value.c_str());

                    auto iter = udf_object_attr_table.find(attr_name);
                    if (iter == udf_object_attr_table.end()) {
                        SWSS_LOG_WARN("Unsupported UDF attribute found %s in %s", attr_name.c_str(), udf_object_name);
                        continue;
                    }
                    sai_attribute_t attr;
                    attr.id = iter->second;

                    UDFGroup udf_group;
                    if (UDF_ATTR_GROUP == attr_name)
                    {
                        if (!getUDFGroupByName(attr_value, udf_group))
                        {
                            SWSS_LOG_ERROR("The referenced UDF group is not found %s for %s", attr_value.c_str(), udf_object_name);
                            valid = false;
                            break;
                        }
                        attr.value.s32 = udf_group.oid;
                    }

                    UDFMatch udf_match;
                    if (UDF_ATTR_MATCH == attr_name)
                    {
                        if (!getUDFMatchByName(attr_value, udf_match))
                        {
                            SWSS_LOG_ERROR("The referenced UDF match is not found %s for %s", attr_value.c_str(), udf_object_name);
                            valid = false;
                            break;
                        }
                        attr.value.s32 = udf_match.oid;
                    }

                    if (UDF_ATTR_BASE == attr_name)
                    {
                        auto offset_iter = udf_offset_base_table.find(attr_value);
                        if (offset_iter == udf_offset_base_table.end())
                        {
                            SWSS_LOG_ERROR("Unsupported UDF offset base found %s for %s", attr_value.c_str(), udf_object_name);
                            valid = false;
                            break;
                        }
                        attr.value.s32 = offset_iter->second;
                    }

                    if (UDF_ATTR_OFFSET == attr_name)
                    {
                        attr.value.s16 = to_uint<uint16_t>(attr_value);
                    }
                    udf_object.udf_object_attrs.emplace_back(attr);
                }
                if (valid)
                {
                    CHECK_ERROR_AND_LOG_AND_RETURN(sai_udf_api->create_udf(&udf_object.oid,
                                                                            gSwitchId,
                                                                            (uint32_t)udf_object.udf_object_attrs.size(),
                                                                            udf_object.udf_match_attrs.data()),
                                                    "Failed to create UDF object %s from SAI call sai_udf_api->create_udf_match" << udf_object_name);
                    m_UDFObjectTable.emplace(udf_object_name, udf_object);
                    SWSS_LOG_INFO("Suceeded to create UDF object %s with object ID %s ", udf_object_name.c_str(),
                                    sai_serialize_object_id(udf_match_oid).c_str());
                }
            }
        }
    }
}