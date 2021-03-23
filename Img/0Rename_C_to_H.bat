cd batch
del *.H

rename *.* *.H

cd..

dir batch\*.* /b > Image0.h

setlocal enabledelayedexpansion


del image.h

for /f "tokens=*" %%i in (Image0.h) do (

set var=#include "batch\%%i"
echo !var! >> Image.h
)

echo 1，使用 Image2Lcd 软件进行批量图片转换
echo 2，使用本脚本批量处理生成的.h文件

echo 按任意键退出
pause