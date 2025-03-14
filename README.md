# StarSimulation

## 技术路线


## 星表参数含义
### Tycho-2
# 表格 2: catalog.dat 文件的字节级描述

| 字节范围 | 格式      | 单位   | 标签       | 解释                                                                 |
|----------|-----------|--------|------------|----------------------------------------------------------------------|
| 1-4      | I4.4      | —      | TYC1       | [1,9537]+= 来自 TYC 或 GSC 的 TYC1                                   |
| 6-10     | I5.5      | —      | TYC2       | [1,12121] 来自 TYC 或 GSC 的 TYC2                                   |
| 12-13    | I1,1X     | —      | TYC3       | [1,3] 来自 TYC 的 TYC3                                              |
| 14-15    | A1,1X     | —      | pflag      | [PX] 平均位置标志                                                    |
| 16-28    | F12.8,1X  | 度     | mRAdeg     | [ ]? α, ICRS 坐标系 J2000.0 年平均位置                               |
| 29-41    | F12.8,1X  | 度     | mDEdeg     | [ ]? δ, ICRS 坐标系 J2000.0 年平均位置                               |
| 42-49    | F7.1,1X   | 毫角秒/年 | pmRA*      | [−4418.0,6544.2]? μα*, ICRS 坐标系 J2000.0 年自行                      |
| 50-57    | F7.1,1X   | 毫角秒/年 | pmDE       | [−5774.3,10277.3]? μδ, ICRS 坐标系 J2000.0 年自行                      |
| 58-61    | I3,1X     | 毫角秒 | e_mRA*     | [3,183]? σα*（基于模型）在平均历元                                   |
| 62-65    | I3,1X     | 毫角秒 | e_mDE      | [1,184]? σδ（基于模型）在平均历元                                    |
| 66-70    | F4.1,1X   | 毫角秒/年 | e_pmRA*    | [0.2,11.5]? σμα*（基于模型）                                          |
| 71-75    | F4.1,1X   | 毫角秒/年 | e_pmDE     | [0.2,10.3]? σμδ（基于模型）                                           |
| 76-83    | F7.2,1X   | 年     | mepRA      | [1915.95,1992.53]? α 的平均历元                                       |
| 84-91    | F7.2,1X   | 年     | mepDE      | [1911.94,1992.01]? δ 的平均历元                                       |
| 92-94    | I2,1X     | —      | Num        | [2,36]? 使用的位置数量                                                |
| 95-98    | F3.1,1X   | —      | g_mRA      | [0.0,9.9]? 平均 α 的拟合优度                                           |
| 99-102   | F3.1,1X   | —      | g_mDE      | [0.0,9.9]? 平均 δ 的拟合优度                                           |
| 103-106  | F3.1,1X   | —      | g_pmRA     | [0.0,9.9]? μα* 的拟合优度                                             |
| 107-110  | F3.1,1X   | —      | g_pmDE     | [0.0,9.9]? μδ 的拟合优度                                              |
| 111-117  | F6.3,1X   | 星等   | BT         | [2.183,16.581]? Tycho-2 B_T_ 星等                                     |
| 118-123  | F5.3,1X   | 星等   | e_BT       | [0.014,1.977]? B_T_ 星等误差                                           |
| 124-130  | F6.3,1X   | 星等   | VT         | [1.905,15.193]? Tycho-2 V_T_ 星等                                     |
| 131-136  | F5.3,1X   | 星等   | e_VT       | [0.009,1.468]? V_T_ 星等误差                                           |
| 137-140  | I3,1X     | —      | prox       | [3,999] 邻近指示符                                                    |
| 141-142  | A1,1X     | —      | TYC        | [T] Tycho-1 恒星标志                                                  |
| 143-148  | I6,1X     | —      | HIP        | [1,120404]? Hipparcos 编号                                             |
| 149-152  | A3,1X     | —      | CCDM       | Hipparcos 恒星的 CCDM 组件标识符                                       |
| 153-165  | F12.8,1X  | 度     | RAddeg     | α, Tycho-2 观测位置, ICRS 坐标系                                      |
| 166-178  | F12.8,1X  | 度     | DEddeg     | δ, Tycho-2 观测位置, ICRS 坐标系                                      |
| 179-183  | F4.2,1X   | 年     | epRA       | [0.81,2.13] RAddeg 的 1990 年历元, ICRS 坐标系                         |
| 184-188  | F4.2,1X   | 年     | epDE       | [0.72,2.36] DEddeg 的 1990 年历元, ICRS 坐标系                         |
| 189-194  | F5.1,1X   | 毫角秒 | e_RA*      | σα*（基于模型）, 观测位置                                             |
| 195-200  | F5.1,1X   | 毫角秒 | e_DE       | σδ（基于模型）, 观测位置                                              |
| 201-202  | A1,1X     | —      | posflg     | [DP] Tycho-2 解的类型                                                 |
| 203-206  | F4.1      | —      | corr       | 相关性, ραδ, 观测位置                                                  |

### 备注

**a** 平均位置标志：
- `J` = 正常平均位置和自行
- `P` = 平均位置、自行等指的是两个 Tycho-2 条目的 B_T 光心
- `X` = 无平均位置，无自行

**b** 拟合优度是散点基误差和模型基误差的比值。仅在 Num > 2 时定义。超过 9.9 的值被截断为 9.9。

**c** 没有星等时为空白。B_T 或 V_T 总是给出。

**d** 以 100 毫角秒为单位到 Tycho-2 主目录或补充目录中最近条目的距离，计算在 1991.25 年历元。如果距离超过 99.9 角秒，则给出 999（即 99.9 角秒）。

**e** `J` = 正常处理，`D` = 双星处理，`P` = 光心处理