## 需求
1.先确定observer的参数、星的参数和渲染类的参数，做出第一张最简单的模拟图

### observer
#### 属性
- RA：赤经
- DEC：赤纬（影响视场中心点）
- FOV：视场角（a*b）
- Time：观测时间（影响星的位置）
- *gamma：噪声指数（0-1）
- *exposure_time：曝光时间（动模糊）
#### 方法
- FilterStar(threshold_magnitude)
  - (根据当前视角先从星表中筛选出符合条件的星)

### star
- RA：此时的赤经
- DEC：此时的赤纬
- Magnitude：星等

### Render
- size：模拟出的星图的尺寸
- Buffer：渲染图像数据缓冲区域
- 