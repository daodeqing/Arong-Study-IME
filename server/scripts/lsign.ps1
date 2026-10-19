# ."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makecert.exe" -r -pe -n "CN=Test ArongStudyImeServer Certificate - For Internal Use Only" -ss PrivateCertStore ArongStudyImeServer.cer
# ."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\certmgr.exe" -add ArongStudyImeServer.cer -s -r localMachine root
# 测试证书的签名
. "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest .\ArongStudyImeServer.manifest -outputresource:.\build\bin\Debug\ArongStudyImeServer.exe; 1
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /fd SHA256 /v /s PrivateCertStore /n "Test Certificate - For Internal scitertest Use Only" /a .\build\bin\Debug\ArongStudyImeServer.exe

# 真实证书的签名
## Debug 版本
### 签名之前要先把 manifest 搞进去(也就是下面的第一步)
. "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest .\ArongStudyImeServer.manifest -outputresource:.\build\bin\Debug\ArongStudyImeServer.exe; 1
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build\bin\Debug\ArongStudyImeServer.exe

## Release 版本
### 签名之前要先把 manifest 搞进去(也就是下面的第一步)
. "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\mt.exe" -manifest .\ArongStudyImeServer.manifest -outputresource:.\build-release\bin\Release\ArongStudyImeServer.exe; 1
."C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe" sign /sha1 "<Your Certum Thumbprint>" /tr http://time.certum.pl /td sha256 /fd sha256 /v .\build-release\bin\Release\ArongStudyImeServer.exe