// GA36-2018 figs2/3/table3/appendixA-h; structural syntax only, not registration validity.
// -1 means preserve the original ordinary/NEV/WJ/legacy-consular compatibility validator.
static int approved_special_syntax(const std::vector<std::string>& c, int max_letters=2) {
    if (c.size() >= 2 && c[0] == "W" && c[1] == "J") return -1;
    bool special = false;
    for (const auto& x : c)
        if (x=="使"||x=="领"||x=="警"||x=="学"||x=="挂"||x=="港"||x=="澳") special=true;
    if (!special) return -1;
    if (c.size()!=7) return 0;
    auto digit=[](const std::string& x){return x.size()==1 && x[0]>='0' && x[0]<='9';};
    auto letter=[](const std::string& x){return x.size()==1 && x[0]>='A' && x[0]<='Z' && x[0]!='I' && x[0]!='O';};
    const std::string provinces="京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼";
    auto province=[&](const std::string& x){return x.size()==3 && provinces.find(x)!=std::string::npos;};
    if(c[6]=="使") { for(int i=0;i<6;++i) if(!digit(c[i])) return 0; return 1; }
    if(c[6]=="领") {
        // Existing old P+letter+4serial+领 acceptance is NOT newly enabled or certified current.
        if(province(c[0]) && letter(c[1])) return -1;
        if(!province(c[0])) return 0;
        for(int i=1;i<=4;++i) if(!digit(c[i])) return 0;
        return digit(c[5])||(max_letters>=1 && letter(c[5]));
    }
    if(c[6]!="警" && c[6]!="学" && c[6]!="挂" && c[6]!="港" && c[6]!="澳") return 0;
    if(!province(c[0])) return 0;
    if(!(letter(c[1]) || (c[6]=="警" && digit(c[1])))) return 0;
    if((c[6]=="港"||c[6]=="澳") && (c[0]!="粤"||c[1]!="Z")) return 0;
    int letters=0;
    for(int i=2;i<=5;++i) { if(letter(c[i])) ++letters; else if(!digit(c[i])) return 0; }
    return letters<=2 && letters<=max_letters;
}
