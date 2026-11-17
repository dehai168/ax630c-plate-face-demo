static bool has(const std::string& s,const char* k){return s.find(k)!=std::string::npos;}
static std::string resolve(const std::string& t,std::string c,const std::string& layer){
 if(has(t,"警"))return "白";
 if(has(t,"使")||has(t,"领")||has(t,"港")||has(t,"澳"))return "黑";
 if(has(t,"学")||has(t,"挂"))return "黄";
 const std::string provinces="京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼";
 static const std::regex ne("[A-Z]([DF][A-HJ-NP-Z0-9]{5}|[A-HJ-NP-Z0-9]{5}[DF])");
 if(t.size()==10 && provinces.find(t.substr(0,3))!=std::string::npos && std::regex_match(t.substr(3),ne))return "绿";
 if(c=="白"||c=="黑")return layer=="double"?"黄":"蓝";
 return c;
}
