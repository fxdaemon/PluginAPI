# PluginAPI

FXDaemon loads the plugin via libPluginProxy, and connects to the broker server with the following configuration.
```
FXDaemon ⇔ libPluginProxy ⇔ Plugin ⇔ Broker
```

The plugin should implement the IBaseOrder interface to be called by FXDaemon, 
and needs to register itself with FXDaemon.  
When a specific event occurs, the plugin can notify FXDaemon via the IPluginProxy interface.

```
IPluginProxy* pluginProxy = getPluginProxy();
pluginProxy->registerPlugin("Order2Rest", this);
pluginProxy->onMessage(MSG_ERROR, "Config file open failed.");
```

### libRestApi

Connect to the broker's trading system using REST-API.  
Please refer to libRestApi/conf/restapi-plugin.cfg, that is defined using the OANDA REST-v20 API as a sample.  
Not all REST-API specifications are supported, so please modify the source if necessary.


### libForexApi

This plugin supports Forex Connect API of FXCM.

To begin using libForexApi, you will need the following:
1. Download "ForexConnectAPI" from [Latest Stable Release (1.5)](http://www.fxcodebase.com/wiki/index.php/Download#Latest_Stable_Release_.281.5.29).
2. Save the unzipped library files to the "bin" directory of FXDaemon.
3. Copy the compiled libForexApi library from libForexApi/bin to the "bin" directory of FXDaemon.
4. Modify libForexApi/conf/forexapi-plugin.cfg according to your account definition and copy to the "conf" directory of FXDaemon.
5. Modify ini file of FXDaemon according to libForexApi/demo-forexapi.ini.

※ If you get a "can't connect to price server" error in Windows 7, please refer to the following:
1. Install "KB3140245" with Windows Update.
2. Add an item to the registry.

| Key | HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings\WinHttp |
|:---|:---|
| Name | DefaultSecureProtocols |
| Value | 0x00000800 |
