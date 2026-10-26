# 原生输入法界面重设计

Mode: Operate. Target: `server/src/settings/native/native_settings_window.cpp`; related surfaces: candidate, toolbar, tray, application assets.

## Direction contract

THESIS: 双色词典。日常中文输入和英文学习共用清晰的文字层级；设置界面的任务导航稳定、操作位置熟悉。

OWN-WORLD: 深青导航区、略带青色的浅底，白色设置区，翡翠色选中态；深色主题使用青黑底与高对比薄荷色强调。24单位图标网格，1.75单位圆头线条，系统中文界面字体。

STORY: 用户先找到输入或翻译设置，再查看状态、调整选项。候选、译文、词卡各自有固定层级，装饰不占用输入路径。

FIRST VIEWPORT: 224 DIP 分组导航，右侧留出28 DIP以上页边距，28 DIP标题与14 DIP正文。设置按功能成组，薄边框代替成片阴影。导航选择保留一处短距离位移动效，降低系统动画时立即定位。

FORM: grounded candidate 5, 双语词典的双色索引。其他候选为交通导览、教材边注、图书目录、城市图书馆导视、语言学习索引卡、字书样张；覆盖印刷、空间导视和屏幕阅读三种来源。seed:ad5366d679a83d49。方向选择页已显示；等待60秒没有回答，本轮按分配方向实现，用户可继续调整。这不是用户已确认的长期偏好。

FINISH: unreviewed and undocumented is unfinished; this build ends with the finish review, the verdict, DESIGN.md, and every shipping raster carrying its provenance

## Constraints

保持原生窗口、现有功能、用户皮肤、翻译设置和输入时序。以真实构建截图和可复现内存测量验收；旧截图仅作对照。
