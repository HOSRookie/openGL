# 项目配置说明

## 签名配置

本项目需要配置 HarmonyOS 签名才能运行。

### 步骤：

1. 复制示例配置文件：
```bash
cp build-profile.json5.example build-profile.json5
```

2. 编辑 `build-profile.json5`，填入你自己的签名信息：
   - `certpath`: 你的证书路径
   - `keyPassword`: 你的密钥密码
   - `profile`: 你的 profile 文件路径
   - `storeFile`: 你的 keystore 文件路径
   - `storePassword`: 你的 keystore 密码

3. 如果你还没有签名证书，请在 DevEco Studio 中生成：
   - File → Project Structure → Signing Configs → 自动生成

**注意**: `build-profile.json5` 已被添加到 `.gitignore`，不会被提交到 Git 仓库。
