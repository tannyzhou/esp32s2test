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

echo 1��ʹ�� Image2Lcd �����������ͼƬת��
echo 2��ʹ�ñ��ű������������ɵ�.h�ļ�

echo ��������˳�
pause