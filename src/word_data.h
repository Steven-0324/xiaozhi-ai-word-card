#ifndef WORD_DATA_H
#define WORD_DATA_H

// ============================================================
// 词库：想加词/改词，直接改下面这个数组，格式 {英文, 音标, 中文释义}
// 注意：中文最高支持常用字库（6845 字），正常背单词的释义都够用
// ============================================================

struct WordEntry {
    const char* word;
    const char* phonetic;
    const char* meaning;
};

static const WordEntry kWordBook[] = {
    {"abandon",    "/əˈbændən/",    "v. 放弃；抛弃"},
    {"affect",     "/əˈfekt/",      "v. 影响；感动"},
    {"effect",     "/ɪˈfekt/",      "n. 效果；影响"},
    {"adequate",   "/ˈædɪkwət/",    "adj. 足够的；适当的"},
    {"ambiguous",  "/æmˈbɪɡjuəs/",  "adj. 模棱两可的"},
    {"benevolent", "/bəˈnevələnt/", "adj. 仁慈的；乐善好施的"},
    {"candid",     "/ˈkændɪd/",     "adj. 坦率的；直言的"},
    {"deliberate", "/dɪˈlɪbərət/",  "adj. 故意的；深思熟虑的"},
    {"elaborate",  "/ɪˈlæbərət/",   "adj. 精心制作的"},
    {"feasible",   "/ˈfiːzəbl/",    "adj. 可行的"},
    {"genuine",    "/ˈdʒenjuɪn/",   "adj. 真正的；真诚的"},
    {"hypothesis", "/haɪˈpɒθəsɪs/", "n. 假设；假说"},
};

static const int kWordBookSize = (int)(sizeof(kWordBook) / sizeof(kWordBook[0]));

#endif  // WORD_DATA_H
