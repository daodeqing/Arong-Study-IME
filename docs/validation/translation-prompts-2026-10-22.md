# 翻译短预设实测（2026-10-22）

模型：workbuddy-intl/deepseek-v4.1-flash。现有 loopback API；所有输入均为本次人工编写的测试文本，不代表真实研究结果。

共 76 次调用：4 次预热、72 次计量；全部 HTTP 200、finish_reason=stop，返回模型标识与请求一致。未发生截断或空响应，未请求第二轮润色或模型打分。
参数：temperature=0、max_tokens=2048、stream=false、thinking=disabled、HTTP timeout=120s。串行、固定种子交错调用；应用译文缓存被绕过。

## 耗时与 token

| 提示词 | 字符数 | 完整响应中位数(s) | 最小–最大(s) | prompt tokens 中位数 | completion tokens 中位数 |
|---|---:|---:|---:|---:|---:|
| 原提示词 | 286 | 1.576 | 1.236–2.294 | 83 | 30.5 |
| 日常沟通 | 281 | 1.610 | 1.237–2.091 | 78 | 30 |
| 学术研究 | 295 | 1.584 | 1.198–2.637 | 81 | 31 |
| 写作创作 | 285 | 1.586 | 1.135–2.540 | 82 | 30 |

预热耗时（未计入上表）：原提示词 3.409s；日常沟通 5.940s；写作创作 11.379s；学术研究 1.442s。
全部响应报告 reasoning_tokens=0、无 reasoning_content；计量请求的 prompt_cache_hit_tokens 均为 0。服务可能进行不透明路由，不能仅凭返回模型字段证明每次物理上游相同。

三套预设的长度与原提示词接近，其中学术预设略长。本次没有支持“短预设明显更快”的证据。token 上限是容量保护，不是固定生成长度；降低上限可能截断，不能作为质量无损的提速结论。

## 每条输入的耗时配对

每格为同一中文在该提示词下三次调用的中位数，单位秒。

| 输入 | 原提示词 | 日常 | 学术 | 创作 |
|---|---:|---:|---:|---:|
| daily_short | 1.599 | 1.595 | 1.549 | 1.536 |
| daily_long | 1.748 | 1.566 | 1.528 | 1.629 |
| academic_short | 1.495 | 1.785 | 1.429 | 1.926 |
| academic_long | 1.623 | 1.909 | 2.354 | 1.482 |
| creative_short | 1.394 | 1.637 | 1.629 | 1.446 |
| creative_long | 1.761 | 1.452 | 1.583 | 1.858 |

## 逐条质量复核

评分由本会话主助手完成，展示材料隐藏预设名称；相同原文与相同译文复用同一判断。共复核 72 条计量响应、41 个不同的原文/译文组合。不是独立专业译者盲评，不用此小样本宣称统计优势。
全部计量译文未发现严重漏译、截断、强化因果结论或新增情节；轻微问题包括 result 译成 results、逗号拼接、遗忘意象弱化，以及“屋里”译成 house。

| 提示词 | 忠实度(0–4) | 事实与术语(0–4) | 场景文风(0–4) | 严重错误 |
|---|---:|---:|---:|---:|
| 原提示词 | 3.83 | 4.00 | 3.83 | 0 |
| 日常沟通 | 3.83 | 3.97 | 3.69 | 0 |
| 学术研究 | 3.78 | 3.94 | 3.78 | 0 |
| 写作创作 | 3.86 | 4.00 | 3.81 | 0 |

以下保留每次计量调用的实际输出，便于复核；评分与具体理由见同目录 quality JSON。

### daily_short

中文：我可能会晚十分钟到，你们先点菜，不用等我。

- 请求 5，学术研究，第 1 次，1.446s，评阅 3430120c7c59：I may be ten minutes late; go ahead and order first, no need to wait for me.
- 请求 19，写作创作，第 1 次，1.631s，评阅 3297f3abb9a8：I might be ten minutes late; go ahead and order without waiting for me.
- 请求 23，原提示词，第 1 次，1.236s，评阅 9dd44baa2637：I might be ten minutes late; go ahead and order first, don't wait for me.
- 请求 27，日常沟通，第 1 次，1.581s，评阅 a54588a8f0b0：I might be ten minutes late. Go ahead and order first, don't wait for me.
- 请求 41，学术研究，第 2 次，1.549s，评阅 75b6af0ce9e3：I may be ten minutes late; go ahead and order without waiting for me.
- 请求 44，日常沟通，第 2 次，1.595s，评阅 a54588a8f0b0：I might be ten minutes late. Go ahead and order first, don't wait for me.
- 请求 50，写作创作，第 2 次，1.536s，评阅 50f73f0eef7f：I might be ten minutes late. Go ahead and order without waiting for me.
- 请求 52，原提示词，第 2 次，1.599s，评阅 d48398170c9d：I might be ten minutes late, so go ahead and order without waiting for me.
- 请求 60，写作创作，第 3 次，1.135s，评阅 89538776b487：I might be ten minutes late; go ahead and order, don't wait for me.
- 请求 66，原提示词，第 3 次，2.294s，评阅 d48398170c9d：I might be ten minutes late, so go ahead and order without waiting for me.
- 请求 71，日常沟通，第 3 次，1.607s，评阅 a54588a8f0b0：I might be ten minutes late. Go ahead and order first, don't wait for me.
- 请求 74，学术研究，第 3 次，1.659s，评阅 e4deef226682：I may arrive ten minutes late; go ahead and order, there is no need to wait for me.

### daily_long

中文：方便的话，能不能把明天下午三点的会议改到周五？我想先确认一下预算，暂时还不能答应这个报价。

- 请求 11，学术研究，第 1 次，1.679s，评阅 24c8287e6b33：If it's convenient, could you move tomorrow afternoon's 3 p.m. meeting to Friday? I want to confirm the budget first and can't agree to this quotation for now.
- 请求 21，原提示词，第 1 次，1.780s，评阅 09ea7e8f21b8：If it's convenient, could you move the meeting at 3 p.m. tomorrow to Friday? I want to confirm the budget first and can't agree to this quote for now.
- 请求 26，写作创作，第 1 次，1.629s，评阅 aa84a433bade：If it's convenient, could we move tomorrow afternoon's 3 o'clock meeting to Friday? I want to confirm the budget first and can't agree to this quote for now.
- 请求 28，日常沟通，第 1 次，1.237s，评阅 5d09f0f16c73：If it's convenient, could you move tomorrow's 3 p.m. meeting to Friday? I want to confirm the budget first, and for now I can't agree to this quote.
- 请求 29，日常沟通，第 2 次，1.566s，评阅 5d09f0f16c73：If it's convenient, could you move tomorrow's 3 p.m. meeting to Friday? I want to confirm the budget first, and for now I can't agree to this quote.
- 请求 34，学术研究，第 2 次，1.292s，评阅 35e2455ed6f5：If convenient, could you move tomorrow afternoon's 3 p.m. meeting to Friday? I want to confirm the budget first and cannot agree to this quotation for now.
- 请求 39，写作创作，第 2 次，1.915s，评阅 08fc222a7240：If it's convenient, could you move tomorrow afternoon's 3 o'clock meeting to Friday? I want to confirm the budget first, and for now I can't agree to this quote.
- 请求 48，原提示词，第 2 次，1.748s，评阅 1524cef2a47c：If it's convenient, could you move the meeting at 3 p.m. tomorrow to Friday? I'd like to confirm the budget first and can't agree to this quote for now.
- 请求 56，写作创作，第 3 次，1.543s，评阅 e8eaa1082de1：If it's convenient, could you move tomorrow afternoon's 3 p.m. meeting to Friday? I want to confirm the budget first and can't agree to this quote for now.
- 请求 64，日常沟通，第 3 次，1.746s，评阅 5d09f0f16c73：If it's convenient, could you move tomorrow's 3 p.m. meeting to Friday? I want to confirm the budget first, and for now I can't agree to this quote.
- 请求 68，学术研究，第 3 次，1.528s，评阅 b6d7d37aafa0：If it's convenient, could you move tomorrow afternoon's 3 p.m. meeting to Friday? I want to confirm the budget first and cannot agree to this quote for now.
- 请求 70，原提示词，第 3 次，1.290s，评阅 c6ca9f505597：If it's convenient, could you move tomorrow's 3 p.m. meeting to Friday? I'd like to confirm the budget first, so I can't agree to this quote for now.

### academic_short

中文：该结果提示两者可能相关，但不能据此推断因果关系。

- 请求 10，原提示词，第 1 次，1.495s，评阅 f25766c88334：This result suggests that the two may be related, but causality cannot be inferred from it.
- 请求 14，日常沟通，第 1 次，1.508s，评阅 ccb54f91032a：The result suggests that the two may be related, but causal relationships cannot be inferred from this.
- 请求 15，写作创作，第 1 次，1.991s，评阅 ccb54f91032a：The result suggests that the two may be related, but causal relationships cannot be inferred from this.
- 请求 16，学术研究，第 1 次，1.198s，评阅 e127c6ca1e11：The results suggest a possible association between the two, but causality cannot be inferred from them.
- 请求 30，原提示词，第 2 次，1.788s，评阅 eaefe765ae54：The result suggests that the two may be related, but causality cannot be inferred from this.
- 请求 31，学术研究，第 2 次，1.429s，评阅 eaefe765ae54：The result suggests that the two may be related, but causality cannot be inferred from this.
- 请求 37，日常沟通，第 2 次，1.849s，评阅 eaefe765ae54：The result suggests that the two may be related, but causality cannot be inferred from this.
- 请求 49，写作创作，第 2 次，1.926s，评阅 ccb54f91032a：The result suggests that the two may be related, but causal relationships cannot be inferred from this.
- 请求 54，学术研究，第 3 次，1.823s，评阅 8a0be24f6286：The results suggest that the two may be correlated, but causality cannot be inferred on this basis.
- 请求 55，日常沟通，第 3 次，1.785s，评阅 03245ec59acf：The results suggest that the two may be related, but causal relationships cannot be inferred from this.
- 请求 57，写作创作，第 3 次，1.215s，评阅 eaefe765ae54：The result suggests that the two may be related, but causality cannot be inferred from this.
- 请求 61，原提示词，第 3 次，1.405s，评阅 ccb54f91032a：The result suggests that the two may be related, but causal relationships cannot be inferred from this.

### academic_long

中文：在三个公开数据集上，我们的方法将误报率从2.4%降至1.8%；由于尚未开展跨中心外部验证，这一结果可能无法直接推广到真实部署环境。

- 请求 9，原提示词，第 1 次，2.024s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 12，日常沟通，第 1 次，1.613s，评阅 9027346f86e6：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not directly generalize to real deployment environments.
- 请求 13，写作创作，第 1 次，1.482s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 18，学术研究，第 1 次，2.354s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 32，学术研究，第 2 次，1.373s，评阅 b38062aa76cf：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not generalize directly to real deployment environments.
- 请求 40，原提示词，第 2 次，1.553s，评阅 9027346f86e6：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not directly generalize to real deployment environments.
- 请求 42，日常沟通，第 2 次，2.091s，评阅 9027346f86e6：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not directly generalize to real deployment environments.
- 请求 45，写作创作，第 2 次，1.366s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 58，写作创作，第 3 次，1.523s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 69，原提示词，第 3 次，1.623s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 72，学术研究，第 3 次，2.637s，评阅 582963757aa0：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not be directly generalizable to real deployment environments.
- 请求 75，日常沟通，第 3 次，1.909s，评阅 9027346f86e6：On three public datasets, our method reduced the false positive rate from 2.4% to 1.8%; because cross-center external validation has not yet been conducted, this result may not directly generalize to real deployment environments.

### creative_short

中文：雨停了，巷口的灯还亮着，像有人把黄昏忘在了那里。

- 请求 7，写作创作，第 1 次，1.352s，评阅 1f05bccd9586：The rain has stopped, and the lamp at the alley entrance is still lit, as if someone had left dusk behind there.
- 请求 8，学术研究，第 1 次，1.585s，评阅 02ace7b724fd：The rain had stopped, and the lamp at the mouth of the alley was still lit, as if someone had left dusk behind there.
- 请求 17，原提示词，第 1 次，1.536s，评阅 6b3f60a7c31f：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone had left the dusk behind there.
- 请求 22，日常沟通，第 1 次，1.643s，评阅 164a3d59c068：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone left the dusk behind there.
- 请求 33，日常沟通，第 2 次，1.637s，评阅 19e6d0fe1b34：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone had forgotten dusk there.
- 请求 35，写作创作，第 2 次，1.446s，评阅 287e5575e3fd：The rain has stopped, but the lamp at the mouth of the alley is still lit, as if someone had left dusk behind there.
- 请求 38，原提示词，第 2 次，1.387s，评阅 1a467dc2ebca：The rain has stopped, and the lamp at the alley entrance is still lit, as if someone had left the dusk behind there.
- 请求 51，学术研究，第 2 次，1.691s，评阅 02ace7b724fd：The rain had stopped, and the lamp at the mouth of the alley was still lit, as if someone had left dusk behind there.
- 请求 62，日常沟通，第 3 次，1.427s，评阅 88fd439495ea：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone had left dusk behind there.
- 请求 65，学术研究，第 3 次，1.629s，评阅 1c192e9114b0：The rain had stopped, and the lamp at the mouth of the alley was still lit, as if someone had left the dusk behind there.
- 请求 67，原提示词，第 3 次，1.394s，评阅 6b3f60a7c31f：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone had left the dusk behind there.
- 请求 76，写作创作，第 3 次，1.916s，评阅 c19a32934ad0：The rain has stopped, and the lamp at the mouth of the alley is still lit, as if someone had left dusk forgotten there.

### creative_long

中文：她把那封没有寄出的信折好，放回抽屉；窗外的风吹动旧窗帘，屋里再没有人提起那个名字。

- 请求 6，写作创作，第 1 次，1.847s，评阅 f6683139b2cc：She folded the unsent letter and put it back in the drawer; the wind outside moved the old curtains, and no one in the house mentioned that name again.
- 请求 20，学术研究，第 1 次，1.583s，评阅 667266c2ea13：She folded the unsent letter and put it back in the drawer; outside the window the wind stirred the old curtains, and no one in the house mentioned that name again.
- 请求 24，原提示词，第 1 次，2.011s，评阅 667266c2ea13：She folded the unsent letter and put it back in the drawer; outside the window the wind stirred the old curtains, and no one in the house mentioned that name again.
- 请求 25，日常沟通，第 1 次，1.322s，评阅 f6683139b2cc：She folded the unsent letter and put it back in the drawer; the wind outside moved the old curtains, and no one in the house mentioned that name again.
- 请求 36，日常沟通，第 2 次，1.452s，评阅 03db41c8fea9：She folded the unsent letter and put it back in the drawer; the wind outside stirred the old curtains, and no one in the house mentioned that name again.
- 请求 43，写作创作，第 2 次，2.540s，评阅 03db41c8fea9：She folded the unsent letter and put it back in the drawer; the wind outside stirred the old curtains, and no one in the house mentioned that name again.
- 请求 46，原提示词，第 2 次，1.761s，评阅 03db41c8fea9：She folded the unsent letter and put it back in the drawer; the wind outside stirred the old curtains, and no one in the house mentioned that name again.
- 请求 47，学术研究，第 2 次，1.550s，评阅 667266c2ea13：She folded the unsent letter and put it back in the drawer; outside the window the wind stirred the old curtains, and no one in the house mentioned that name again.
- 请求 53，日常沟通，第 3 次，1.614s，评阅 03db41c8fea9：She folded the unsent letter and put it back in the drawer; the wind outside stirred the old curtains, and no one in the house mentioned that name again.
- 请求 59，原提示词，第 3 次，1.398s，评阅 667266c2ea13：She folded the unsent letter and put it back in the drawer; outside the window the wind stirred the old curtains, and no one in the house mentioned that name again.
- 请求 63，写作创作，第 3 次，1.858s，评阅 03db41c8fea9：She folded the unsent letter and put it back in the drawer; the wind outside stirred the old curtains, and no one in the house mentioned that name again.
- 请求 73，学术研究，第 3 次，1.848s，评阅 667266c2ea13：She folded the unsent letter and put it back in the drawer; outside the window the wind stirred the old curtains, and no one in the house mentioned that name again.

## 建议与边界

- 本轮保留产品原提示词，三种预设仅作为待选方案；场景表现接近，尚不足以证明某个预设稳定胜出。
- 优先保持单次请求、关闭思考、一个完整译文；术语表可按实际命中少量附加到同一请求，先验证再接入。
- 连接复用可以减少连接建立开销，但 loopback 第一跳很短，应先测云端链路，不能预设大幅收益。
- 保留现有重复确认复用、过期请求取消、防抖与有界缓存；今后接入预设时缓存键必须包含模型、预设和原文，并随配置变化失效。
- 流式输出适用于预览首次出现；应等完整响应校验后一次提交，不能把首个 token 耗时当完整上屏耗时。
- 本评测没有衡量宿主 UI 线程、输入法切换时延或 CPU；不能用本结果证明按键卡顿已解决。

## 可复现

使用 D 盘 Python：python -B scripts/benchmark_translation_prompts.py --config <现有配置路径> --output docs/validation/<新文件名>.json。
脚本不会修改或复制配置，不记录 token，也不会覆盖已有结果。原提示词直接从产品源码提取，源码 SHA-256、完整参数、请求 ID 与 token 用量均保留在结果 JSON。
