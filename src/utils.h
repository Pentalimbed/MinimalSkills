#pragma once

namespace minskill
{
union ConditionParam
{
    char         c;
    std::int32_t i;
    float        f;
    RE::TESForm* form;
};

template <class T>
T* getForm(const std::string& plugin_name, RE::FormID form_id)
{
    auto data_man = RE::TESDataHandler::GetSingleton();
    auto result   = data_man->LookupForm(form_id, plugin_name);
    if (!result)
    {
        logger::error("Failed to find form {:x} in {}", form_id, plugin_name);
        return nullptr;
    }
    if (result->formType != T::FORMTYPE)
    {
        logger::error("Form {:x} in {} doesn't match the required type!", form_id, plugin_name);
        return nullptr;
    }
    return result->As<T>();
}

void parseStrList(std::vector<uint16_t>& vec, const std::string& str)
{
    std::stringstream strstrm(str);
    uint16_t          tmp;
    while (strstrm >> tmp)
        vec.push_back(tmp);
}

} // namespace minskill