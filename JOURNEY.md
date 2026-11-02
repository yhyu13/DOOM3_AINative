# JOURNEY — DOOM3_AINative

> 结论先行：这个项目真正交付的不是“一个现代 DOOM 3 渲染器”，而是 *一条可复跑的工程闭环*——原生 x64 基线已能构建、启动、截图、存读档、录放 demo 和本机回环联机；渲染器现代化走到 Q4/Q6/Q7 的受限验收；引擎本体的 WebAssembly 移植尚未开始。项目最值钱的部分，是它把“没跑”与“跑通”分开记录的习惯。

投入：<u>7 天（2026-10-26 至 2026-11-02，按经过时间计）</u>、<u>94 个提交</u>、<u>10 份 spec</u>。产出：<u>1 个可运行的 x64 引擎基线</u> + <u>1 套可复跑的证据闸门</u>。回收：后续每个渲染或移植切片复用同一套 issue → worktree → PR → 独立评审 → 证据流程，减少重复自证与返工。

## 读法

- `ME = 用户`：提出目标、做取舍、纠偏、叫停、授权。
- `YOU = AI`：实现、测量、证伪、写证据、报告“没跑”。
- 日期均为 2026 年，格式 `YYYY-MM-DD`。
- 数字带单位；`NOT RUN`、`SKIPPED`、`PARTIAL`、`ACCEPTED WITH LIMITS` 都不是 `PASS`。

## 风险与待办

| 风险 / 待办 | 纪元 | 证据与状态 |
|---|---|---|
| 零售 *Resurrection of Evil* 未验证；免费 demo 的再分发条款未确认；x64 存档与 32 位存档不兼容 | 纪元 2 | `targets/win11-amd64/README.md` §Known state；`SAVEGAME_VERSION` 18 |
| 展示页第 2 页的许可闸门 T042 未解除；公开版里的真实帧是脱敏占位 | 纪元 3 | `specs/002-project-showcase/spec.md`；`specs/007-aigc-vision/spec.md` |
| Q1 全量仍 `INCOMPLETE`：T031 的独立评审已提交（#117），但卡在“skipped debug layer 被当成 active”；#123 已修，等待重审。Q2 的 fork selector 已随 submodule bump 落地，但 T040/T083 仍开。 | 纪元 4 / 7 / 10 | `review-q1-final.md`；`tasks.md` T031/T036/T040/T083；#113、#117、#123 |
| `docs/ROADMAP.md` §7 仍记录 `f9e84c8` 的两道红闸门，但 #115 已把它们修绿；文档与代码漂移 | 纪元 5 / 10 | #115 提交正文：RHI header gate exit 0、WebGPU mapping <u>119/119</u> |
| #104 合并闸门事故：<u>3 个 PR</u> 在独立评审前被 squash 合并；补救已合，issue 未关，且不能追溯合规 | 纪元 6 / 10 | issue #104；#101 的 `APPROVED` 比合并晚 <u>23 分钟</u> |
| 会话与工作树膨胀：清理前 <u>20 个 worktree</u>、<u>24 个本地分支</u>、<u>30 个远程分支</u>；清理后 <u>3 / 4 / 2</u> | 纪元 6 | 2026-10-29 清理会话；本地可召回 <u>187 个会话</u> |
| Q4 是 `ACCEPTED WITH LIMITS`，不是 green；无引擎帧、单主机、程序化角色与玻璃、无天空与历史 | 纪元 8 | `specs/004-quality-renderer/review-q4-r3.md` §6、§10 |
| Q5 `NOT RUN`：无 NVIDIA GPU / SDK；Q6 已有 DXR 合成捕获，但 T069 `PARTIAL`、T070 `NOT RUN`；Q7 T072/T073 完成，T074/T075 `NOT RUN` | 纪元 9 / 10 | issue #105；`evidence-q6.md`；`evidence-q7.md` |
| 008 有 <u>5 个任务</u> 阻塞在授权零售数据；009 的 Q4 capture 同样阻塞 | 纪元 7 / 9 | issue #105；`specs/008-classic-shaders/spec.md`；`specs/009-rhi-hd-content/spec.md` |
| 引擎本体的 WebAssembly 端口未开始：`neo/` 下 <u>0 个</u> emscripten 引用；现有 `web/` 是 Q7 host slice，不是引擎端口 | 纪元 10 | `docs/ROADMAP.md` §1；`evidence-q7.md` |
| Q3 的便携半已通过，但 engine-fork 半仍阻塞：T044/T085 需要引擎 fork 的写入点，T048 的 moving-scene 与独立 `review-q3.md` 未跑 | 纪元 7 / 10 | `tasks.md` T044/T048/T085；`evidence-q3.md` |
| 本 `JOURNEY.md` 尚未独立评审；已进入 `docs/125-project-journey` worktree，等待 PR 评审 | 纪元 10 | 当前工作树 |
| 公开仓库的 GPL / 资产边界仍在：只能放源码与证据，零售 `.pk4` 不入库 | 全项目 | `AGENTS.md` 非协商项 3；`docs/ROADMAP.md` §2 |

## 纪元 1 — 2026-10-26 立项：先把工作方式搭起来

| ME = 用户 | YOU = AI |
|---|---|
| “0 make git repo and add doom3 submodule as 1 setup speckit 2 and multiagent issue pr worktree git worflow guidance doc 3 AI Native game dev skill install for this local project from already own skills ( mainly for AI migration game to e.g. web, AI testing, automation, AI coding, etc) if lacking search internet for ONLY valid skills 4 you suggest” | 提交脚手架：<u>86 个文件</u>、<u>10,240 行</u>；把 `doom3_orig/doom3/` 固定为子模块 `a9c49da`；写 `AGENTS.md`、`docs/GIT_WORKFLOW.md`、`docs/SKILLS.md`；安装 spec-kit 与 <u>25 个</u> 项目 skill。 |
| “忘记说的事情，需要你额外记得做：把我们移植到 win11 64 的经验和调试技巧总结成可以服用的 skill 放到这个仓库中去……注意要放到 `.claude` 目录下，并且确保 skill 对仓库内 agent 可见。” | 生成 `win64-port-playbook`；建立 `.claude/skills`（tracked）与 `.kilo/skills`（git-ignored 镜像）；写 `scripts/sync-skills.ps1`、`verify-skills.ps1`。 |
| （未额外指示） | 证据盘点：`base/` 只有 <u>14 字节</u> 的 `default.cfg`，<u>0 个</u> `.pk4/.pk3/.pak`。这条事实后来决定了每一份 spec 都必须把“代码能跑”和“有合法数据可跑”分开写。 |

## 纪元 2 — 2026-10-26～27 原生 x64：先让引擎跑起来

| ME = 用户 | YOU = AI |
|---|---|
| “Adapt DOOM 3 to the current Win64 / Windows 11 system so it runs on this computer, in `targets/win11-amd64`.” | 面对 <u>1,550 个</u> MSVC inline `__asm`、VS2010 解决方案、DirectX June 2010 SDK 与 32 位 OpenAL，改成 CMake + VS2022 v143 + x64。首轮 <u>59 个文件</u> 修改，产出 `DOOM3.exe` <u>16,690,688 字节</u>，PE machine `0x8664`；demo 地图加载 <u>1969 个 entity</u>，截图有 <u>826 种</u> 不同颜色。 |
| （要求“能跑”，不是“能链接”） | 指针宽度、`idEventArg`、`wexpOp_t`、Heap、SIMD、SEH、脚本 `OP_ADDRESS` 逐项修；保存/读档把 `(int)ptr` 改成显式 presence flag，`SAVEGAME_VERSION` 升到 18；新增 `sys_cpuInfo`、`sys_fpuState`、`sys_stackTrace`；本机回环联机到 in-game 状态。 |
| （未额外指示） | 独立评审给出 *1 BLOCKER · 4 MAJOR · 6 MINOR*：补丁从未在 setup 里被应用、SC-003 没有被断言、demo 的指针仍被截断、日志是旧的。修复后补丁集从 <u>59 个文件</u> 增到 <u>74 个文件</u>（<u>768,242 字节</u>），可从 pristine 逐字节重建。 |
| （要求附带 RoE 能力） | 同一构建产出 `DOOM3XP.exe`：<u>71 个</u> d3xp TU 静态链接；能启动、能进 demo 地图；零售 RoE 内容与真实 MP 仍标为未验证。 |

## 纪元 3 — 2026-10-27～28 展示：把“能跑”讲成“愿景”

| ME = 用户 | YOU = AI |
|---|---|
| “Write an attractive single interactive HTML with voice-over, using a skill, to show this project to others. Three rotating poster pages. Use a real DOOM 3 frame inside a browser/monitor, plus marketing concepts like key remapping, mods, and one-sentence AI Rogue generation.” | 做 3 页单文件 HTML deck；许可审计发现没有可合法复用的 DOOM 3 实机截图，于是公开版用 *脱敏真实帧*（16:9 裁切、48×27 马赛克、`XX` 网格、`REDACTED` 条），原图不入库。首轮验收 <u>124 项</u> 契约检查 + <u>161 项</u> 深度检查 + axe <u>0 violations</u>。 |
| “第二页重做，讲出了源码干什么？谁看的动？玩家要的是我们的想象力不是我们的困难，把我们这个 web doom3 现代化 AI 改造成现代 FPS 画质然后 AI native 定制游戏体验的 idea 卖出去！” | 把第 2 页从“源码/架构说明”改成“2026 现代 FPS 愿景”；第 2/3 页接 GPT Image 2.5（图生图 + 文生图），保留 `--aigc-hero` 可选与程序化回退。 |
| （要求“可见性证据必须真实”） | 合并后的独立评审指出“可见性证据太弱”：于是补 computed-style + 像素相关性闸门和负向控制，验收升到 `verify.py` <u>171/0</u>、`verify_deep.py` <u>186/0/1</u>、axe <u>0</u>。第 2 页的 T042 许可闸门保留，没有用“看起来没问题”替代。 |

## 纪元 4 — 2026-10-27 路线转向：从“换底座”到“自研 RHI + D3D12”

| ME = 用户 | YOU = AI |
|---|---|
| “为了 Win11 AMD64 贴图着色器现代化高清化，可能渲染后端也要升级（Vulkan/DX12？），需要源码抽象出 RenderRHI？以后 Web 端甚至移动端都可能用到。系统研究 Doom 3 引擎升级画质，以及不升级引擎只升级材质（包括着色器），或者用 DLSS 5 或其他 AI 渲染方案。” | 003 研究先给出“不要先做 Vulkan/DX12，也不要做 DLSS”的结论：成本中心是 2004 年的 shader 与状态模型；建议 GL 3.3 core / GLSL，一次重写覆盖 Windows、WebGL2、GLES3；DX12 直接排除。 |
| “review specs/003-render-modernization/research.md 重新列出最短路劲最长路径，我们有最强的 AI，不要在乎时间，哪个做法最出彩，比如自己做 RHI 构建 最基本的 DX12 后端（当然要支持 MV 或 deferred shading 和 DLSS5) 作为设计基础” | 004 取代 003 的冲突决策：自研小型 RHI，D3D12 先行，hybrid renderer 做可玩基础，path tracing 做上限；DLSS 5 只作可选 provider，不得阻塞独立渲染器；003 标为 historical，并把 1–2 天 / 16–32 工程师周的估算撤回。 |

## 纪元 5 — 2026-10-28～29 契约先行：Q1a 的教训

| ME = 用户 | YOU = AI |
|---|---|
| 用户要求按 issue → worktree → PR → 独立评审推进（转述自纪元 1 的工作方式要求） | 先做 S0：把 RHI 契约拆成 *portable core / D3D12-private / native extension* 三层；T004/T005 必须由非作者签核，签核前不宣布 S0 完成。 |
| （未新增原话） | 发现 Q1a 的 PR #18 是基于 *合并前* 的旧契约写的；如果直接合并，会把 #26 刚修掉的 D3D12 泄漏重新带回来。协调结论：先不要合并 #18，也不要宣布 S0 完成。 |
| （未新增原话） | 发现当前 GitHub 身份只有 `READ`：`git push` 返回 403，`gh issue edit` 被拒。没有绕过，改为把分支留在本地并明确报告阻塞。 |
| （未新增原话） | 词法闸门跑出 <u>26 处</u> “prohibited D3D12 vocabulary in engine-facing header”；WebGPU 映射闸门报 <u>62 个问题</u>、<u>57/119</u> 个声明已映射。这两道红闸门后来成为 #115 的收口对象。 |

## 纪元 6 — 2026-10-29～30 多会话工程：把 AI 当团队管

| ME = 用户 | YOU = AI |
|---|---|
| “我想给这个项目 agent 更多自由，一个就是 agent 可以自己决定合并进入主干，只要独立 subagent review 没有问题且 merge 没有 conflict 就行，你写入 ag md 并提交主干 然后通知 agentmanager” | 把自合并写成 `AGENTS.md` §11：独立非作者评审、自己的 Agent Manager session/worktree、只拿 diff/spec/验证命令、重跑、在 PR 评论里写 `APPROVED` + reviewer session ID + reviewed SHA；head 与 merge base 任一变化都作废旧批准。 |
| “ok pr 合并更新 issue 不要关闭 issue” | 把“不擅自关 issue”写成非协商项；PR 用 `Refs #N`，不用 `Closes`。 |
| “不应该在 worktree 工作吗？继续实现与 human review；runtime census/legal 仍阻塞完整实现” | 把“从 main 开 branch/worktree、不在脏 main 上直接 merge”写进流程；共享文件（`src/renderer/rhi/**`、target CMake、`tests/renderer/CMakeLists.txt`）串行；同一时刻最多 <u>3–4 个</u> 活跃会话。 |
| （要求把多会话跑顺） | 写 `docs/AGENT_MANAGER_CRON.md`：每 <u>5 分钟</u> 扫一次会话与 PR，只合已审、当前、过闸的 PR；主分支前进导致批准级联失效；停滞会话先保 WIP，再开一个单写者接手。 |
| （未新增原话） | 清理 <u>17 个</u> worktree、<u>20 个</u> 本地分支、<u>28 个</u> 远程分支；把“会话会膨胀”变成可执行的清理动作，而不是口头提醒。 |

## 纪元 7 — 2026-10-29～30 渲染切片：Q1b / Q2 / Q3 / 008

| ME = 用户 | YOU = AI |
|---|---|
| “你要做到我满意，对于当前会话 spec do 自我验收，用所有可能方法，测试，playwright，截图等等 do 自我 review 一遍当前 spec do 然后当前会话 spec impl 所有 answer 然后回答当前会话 spec 满足所有 task 了吗？如果有没完成的列出来，下次完成……多用 subagent 并行加速……列表格” | 把这句话当成验收模板，逐会话输出“问题 / plan / task / 完成度 / 状态 / 未完成项”。Q1b 做到 <u>32/32</u> CTest（Debug 与 Release）；Q3 做到 <u>23/23</u>（其中 <u>3 个</u> 是 `WILL_FAIL` 的预期红）；Q2 的 ordered scene adapter 合并，但 Q2 证据仍 `NOT RUN`。 |
| （要求“别把设计当实现”） | Q0 四类参考场景里，open view 两次运行差异 <u>64–72%</u>，close character 差异到 <u>93%</u>，于是标 `SKIPPED`；不把“看起来像”写成 PASS。 |
| （要求 shader 不要靠猜） | 008 先做 census + provenance + fail-loud resolver；<u>6 个</u> 程序能编译到 DXIL / SPIR-V / WGSL；resolver 接进引擎；但 <u>11 个</u> 任务只完成 <u>6 个</u>，<u>5 个</u> 阻塞在“授权数据 + 可运行的 classic backend”，没有一个程序升到 `owned`。 |

## 纪元 8 — 2026-10-30～31 Q4：七格内容与三次艺术评审

| ME = 用户 | YOU = AI |
|---|---|
| 用户要求资产自制、可公开、不碰零售数据（转述自 009 boundary：CC0、每个资产带 provenance、仓库只放可重建的源与清单） | 做 Q4 独立 harness：<u>7 格内容 / 4 类</u>，<u>11 条</u> capture 记录；修 B-1…B-6（alpha-tested cutout shadow、dielectric 玻璃、环境 sidecar、census 规则、provenance 扩展名、重复 key）。 |
| （要求“把画质当产品验收”） | 独立艺术评审跑三轮：r1 发现 cutout shadow 与玻璃问题；r2 指出 VRAM peak 未测、不能用 adapter 的 <u>134,217,728 字节</u> dedicated 冒充 use；r3 实测 process LOCAL `CurrentUsage` 峰值 <u>107,110,400 字节</u>，<u>12/12</u> 次运行一致，落在 `materials_loaded`。 |
| （未新增原话） | r3 结论是 `Q4: ACCEPTED WITH LIMITS`，并明确写“这不是 unrestricted Q4-green”；程序化角色、屏幕空间玻璃、SDR emissive、无天空、无 history、单主机都列为 *limits*，不升级为 pass。 |

## 纪元 9 — 2026-10-30～11-01 Q6 / Q7 / Q5：把“没跑”写成结论

| ME = 用户 | YOU = AI |
|---|---|
| 用户授权 provider 工作，但授权不能变出 GPU / SDK（#105 转述） | Q5 只交非专有 off path：<u>35 项</u> 检查、与关闭态逐字节一致；NVIDIA SDK / 设备 / create-evaluate 全部 `NOT RUN`；issue #105 把“无零售数据、无 NVIDIA GPU/SDK”写成阻塞，而不是降级检查。 |
| （要求 path tracing 做出参考实现） | Q6 的 CPU reference 在 Debug/Release 各跑 <u>249 项</u> 检查、<u>5/5</u> CTest；DXR probe 报到 `RaytracingTier = 1_1`、device/BLAS/TLAS 创建、<u>64/64</u> trivial dispatch。新增 `tests/dxr-capture` 在真实 DXR 设备（Intel Graphics，tier 1.1，SM 6.6）上渲 320×180 的静态、刚体、蒙皮、alpha/emissive、透射夹具：<u>10 条</u> capture、<u>13 张</u> PNG 两次运行逐字节一致，最差 `mae` <u>0.001309</u>、`p99` <u>0.040979</u>、flat-field denoise 比 <u>0.0810</u>，640×360/1 spp 的帧时间中位数 <u>5.39 ms</u>、p95 <u>6.61 ms</u>。但这些是合成夹具，不是 Q4 内容，也不是 Q6 引擎帧；PT-S1..S5、输入延迟、VRAM、temporal history 仍 `NOT RUN`，T069 仍 `PARTIAL`，T070 `NOT RUN`。 |
| （未新增原话） | 发现 Release denoiser 是 MSVC 19.38 的 codegen 缺陷：depth-guided 分支把 R/G 累加器整个丢掉，*每个像素* 的 R/G 都是 0；旧的 far-side 检查同时接受正确值 <u>0.267982</u> 和错误值 <u>0.019348</u>（差 <u>13.8 倍</u>），所以“44/45 通过”没有保护它声称保护的东西。修复用 float64 参考值与逐通道容差，检查变严而不是变宽。 |
| （未新增原话） | Q7 映射闸门一度只覆盖 <u>57/119</u> 个声明；#115 补到 <u>119/119</u>，但明确记录这是 *classification*，不是浏览器重跑。T072 的 portable fixture 做到 <u>6/6</u> CTest、<u>537 项</u> 检查；T073 的 `webgpu-browser` adapter 在 pinned Chrome 156 上两次得到 <u>11 PASS / 0 FAIL / 4 NOT RUN</u>；T074 仍因无合法游戏数据 `NOT RUN`，T075 仍等独立评审。 |

## 纪元 10 — 2026-11-01～02 收口：闸门、事故与当前边界

| ME = 用户 | YOU = AI |
|---|---|
| （未新增原话） | #101 跑真正的 D3D12 debug layer + GPU-based validation，抓到并修掉 Q1b 的 optimized-clear 不匹配与 resize/destroy 时 in-flight back-buffer release（layer `id=921 CORRUPTION`）；waiver 从“按 ID + severity 无条件放行”收紧为作用域控制（#106）；#108 收紧 DXR probe pass 契约；#109 让 manifest coverage fail-closed 并把 T077 拆诚实；#110 修 denoiser；#115 把 Q4 harness 归为 private tier，并补完 WebGPU 映射。 |
| （未新增原话） | #104 记录一次真实事故：<u>3 个 PR</u> 在独立评审前被合并，时间窗 <u>06:15:59–06:16:21</u>；#101 的 `APPROVED` 比合并晚 <u>23 分钟</u>，#102 完全没有评审。补救 PR 已合，但 issue 明确写“不能追溯批准”，不把事故洗成合规。 |
| （未新增原话） | #117 提交 Q1 的最终独立评审，结论是 blocked：fixture 把 requested-but-skipped debug layer 当成 active；#123 改成只按 `CheckState::Run` 计 active，layer-absent 主机上 Q1 CTest <u>17/17</u>、全套 CTest <u>38/38</u>，T031 可重审但未勾。 |
| （未新增原话） | #113 把子模块从 `a9c49da` 升到 `cd6ae660`，PR body 带 owner sign-off “需要人授权的我都授权了”；fork PR #1 的 Q2 selector 因此进入主树，`DOOM3.exe` 里有 `renderapi_status`，但 T036 的 failed-initialization 测试与 T083 的集成检查仍未跑。 |
| （未新增原话） | #118 在 DXR 设备上补 Q6 合成捕获；#119 完成 Q7 T072/T073 并记录 T074/T075 未跑；#121 让 Q3 的便携半走通 Q2 有序流（T044 <u>58 项检查/0 失败</u>、T085 <u>211 项检查/0 失败</u>），engine-fork 半仍 blocked。 |
| “主干需要一个 journey md 用 journey skill” | 写这份 `JOURNEY.md`：以 commit、spec、evidence、issue 为骨架，不用聊天记录替代证据。 |
| “journey pr 到主干上” | 开 issue #125，从 `origin/main` 建 `docs/125-project-journey` worktree；把 JOURNEY 从主工作树移入，按最新 main 更新账本后提交 PR。 |

当前账本：`origin/main` 上 <u>94 个提交</u>；本地可召回 <u>187 个会话</u>；<u>10 份 spec</u>；open PR 有 #122、#124（docs 对账）与本 PR；T030 已勾、T031 已审但待重审；Q1 `INCOMPLETE`、Q2 `NOT RUN`（selector 已落地，T040/T083 未关门）、Q3 `PARTIAL`（便携半通过，engine-fork 半 blocked）、Q4 `ACCEPTED WITH LIMITS`、Q5 `NOT RUN`、Q6 `PARTIAL`（已有 DXR 合成捕获，T070 未跑）、Q7 `PARTIAL`（T072/T073 完成，T074/T075 未跑）。

## 这个项目如何教 vibe coding with AI

### 人的工作

1. *先定协作方式，再写第一行业务代码*。开局的 “0 make git repo … 1 setup speckit 2 multiagent issue pr worktree …” 把仓库结构、评审方式和 agent 权限一次定下；后面所有加速都建立在这条骨架上。
2. *一句话改叙事*。“第二页重做……玩家要的是我们的想象力不是我们的困难”不是排版意见，而是把展示从“源码说明”改成“产品愿景”；AI 随后重做页面并接入 AIGC，而不是继续解释代码。
3. *用授权换速度，但保留红线*。“agent 可以自己决定合并进入主干，只要独立 subagent review 没有问题且 merge 没有 conflict” 直接催生 §11；但 submodule bump、policy change、关闭 issue 仍留给人。
4. *用“做到我满意”逼出验收*。那句反复出现的验收模板要求自我验收、自我 review、列未完成项、用 subagent 并行；它把“做完了”从感觉变成清单。
5. *选“最出彩”而不是“最省时”*。“我们有最强的 AI，不要在乎时间，哪个做法最出彩” 把路线从“换 dhewm3 底座”推到“自研 RHI + DX12”；代价是 AI 必须同时守住 claim boundary，不能把设计稿说成已实现。
6. *该叫停就叫停*。“不应该在 worktree 工作吗？”和“不要关闭 issue”都是短句，但都变成了可执行的仓库规则；一次纠正的杠杆远大于十次解释。

### AI 的工作

1. *先造度量工具，再谈结论*。`verify-win11-amd64.ps1`、Q0 捕获脚本、CTest、RHI/WebGPU 闸门、VRAM probe 都是先于结论存在的；没有它们，截图和“看起来对”会变成唯一证据。
2. *把“不可用”写成 `SKIPPED` / `NOT RUN`*。Q0 的 open view / close character、Q5 的 NVIDIA 路径、Q6 的引擎帧视觉行、Q7 的真实场景都拒绝用 `PASS` 代替。
3. *证伪自己的实现*。denoiser 的 R/G 全零、VRAM 用 adapter 容量冒充、Q4 的“不是 green”、#104 的合并事故，都是 AI 自己记录并公开的负面结论。
4. *把评审变成可执行的边界*。waiver 作用域、mutation controls、private tier、fail-closed manifest、同 SHA 重审，都是把“请小心”翻译成脚本能拒绝的条件。
5. *保持账本*。`tasks.md`、`traceability.md`、`evidence-*.md`、`review-*.md` 让“完成度”有行号、SHA、退出码，而不是会话里的一句“差不多了”。

### 可复用的规则

1. *先定契约，再写第一行实现*。Q1a 基于旧契约开发，差点把 D3D12 泄漏带回 main；三层契约与 T004/T005 签核把返工挡在合并前。
2. *每个结论配一条可重跑的命令*。构建 exit code、CTest 计数、闸门 exit code、捕获脚本输出，都是结论的一部分。
3. *`NOT RUN` / `SKIPPED` 不是 `PASS`*。Q0 的 64–93% 差异、Q5/Q6/Q7 的未跑项，都按未跑记录。
4. *测量值不能拿容量冒充*。adapter 的 <u>134,217,728 字节</u> dedicated 不是 process VRAM peak；真正的峰值是实测的 <u>107,110,400 字节</u>。
5. *一个测试如果对错都通过，就不是测试*。denoiser 的 `luma < 0.35` 同时接受 <u>0.267982</u> 和 <u>0.019348</u>；后来改成参考值 ± 容差，并加 flat-field 控制。
6. *评审必须独立、同 SHA、自己的 worktree*。§11 的四条同时成立才算批准；#104 证明“先合后审”无法追溯修复。
7. *合并前先清 main 的脏状态*。mergeability 只看远端 head，看不见本地脏文件；本轮把 `JOURNEY.md` 从主工作树移入 `docs/125-project-journey`，main 才恢复干净。
8. *共享文件串行，按 issue/PR 并行，不按 spec 数量并行*。`rhi.h`、target CMake、`tests/renderer/CMakeLists.txt` 都曾是需要单写者的点；同一时刻 <u>3–4 个</u> 会话是经验上限。
9. *一次纠正沉淀成规则或技能*。“移植经验做成 skill” 产出 `win64-port-playbook`；自合并要求产出 §11；worktree 纠正产出开工检查。
10. *文档会漂移，收口时重读代码与闸门*。ROADMAP 仍写着旧红闸门，而 #115 已修绿；这类漂移只能靠收口时重跑命令解决。
11. *资产与许可先于画质*。没有合法零售数据，008 的 <u>5 个任务</u>、009 的 capture、Q5 的 provider 都只能停在 `NOT RUN`；这不是代码缺陷，不能靠放宽检查“修复”。

### 一句话总结

人负责选方向、划红线、判断“够了”；AI 负责把方向变成可复跑的证据，并在证据不足时说出“没跑”。

## 证据索引

- 工作方式与红线：`AGENTS.md`、`docs/GIT_WORKFLOW.md`、`docs/AGENT_MANAGER_CRON.md`
- 当前能力边界：`docs/ROADMAP.md` §7、`README.md` “What works today”
- 原生 x64：`targets/win11-amd64/README.md`、`specs/001-win11-amd64-native-build/`
- 渲染器：`specs/004-quality-renderer/`（`spec.md`、`tasks.md`、`evidence-*.md`、`review-q4-r3.md`）
- 事故与阻塞：issue #104、issue #105
- 迁移快照：`specs/010-migration-snapshot/`

> 本输出由 AI 辅助生成，仅用于信息整理与研究参考，不构成专业意见或决策依据。
