#!/usr/bin/env python3
"""
BLE测试脚本 - 用于验证腕带BLE通信
功能：模拟MIT App Inventor APP接收腕带数据
"""

import asyncio
import sys
import json
from bleak import BleakScanner, BleakClient

# BLE UUID配置（与腕带代码一致）
SERVICE_UUID = "a1b2c3d4-e5f6-4789-abcd-ef0123456789"
CHARACTERISTIC_UUID = "a1b2c3d4-e5f6-4789-abcd-ef012345678a"
DEVICE_NAME = "DiabetesSensor"

class BLETester:
    def __init__(self):
        self.client = None
        self.connected = False
        self.received_data = []
        
    def notification_handler(self, sender, data):
        """处理接收到的BLE通知数据"""
        try:
            # 解码JSON数据
            json_str = data.decode('utf-8')
            json_data = json.loads(json_str)
            
            print(f"\n📡 收到BLE数据:")
            print(f"   心率: {json_data.get('hr', 'N/A')} bpm")
            print(f"   血氧: {json_data.get('spo2', 'N/A')}%")
            print(f"   丙酮: {json_data.get('acetone', 'N/A')} ppm")
            print(f"   备注: {json_data.get('note', 'N/A')}")
            
            self.received_data.append(json_data)
            
        except Exception as e:
            print(f"❌ 数据解析错误: {e}")
            print(f"原始数据: {data.hex()}")
    
    async def scan_devices(self):
        """扫描BLE设备"""
        print("🔍 正在扫描BLE设备...")
        
        devices = await BleakScanner.discover(timeout=10.0)
        
        target_devices = []
        for device in devices:
            if device.name and DEVICE_NAME in device.name:
                print(f"✅ 找到目标设备: {device.name} ({device.address})")
                target_devices.append(device)
            elif device.name:
                print(f"  其他设备: {device.name} ({device.address})")
        
        return target_devices
    
    async def connect_and_listen(self, device):
        """连接设备并监听通知"""
        print(f"\n🔗 正在连接设备: {device.name} ({device.address})")
        
        try:
            self.client = BleakClient(device.address)
            await self.client.connect(timeout=15.0)
            self.connected = True
            
            print("✅ 连接成功")
            
            # 获取服务
            services = await self.client.get_services()
            print(f"📋 发现 {len(services.services)} 个服务")
            
            # 查找目标服务
            target_service = None
            for service in services:
                if str(service.uuid).lower() == SERVICE_UUID.lower():
                    target_service = service
                    print(f"✅ 找到目标服务: {service.uuid}")
                    break
            
            if not target_service:
                print("❌ 未找到目标服务")
                return False
            
            # 查找特征值
            target_char = None
            for char in target_service.characteristics:
                if str(char.uuid).lower() == CHARACTERISTIC_UUID.lower():
                    target_char = char
                    print(f"✅ 找到目标特征值: {char.uuid}")
                    print(f"   属性: {char.properties}")
                    break
            
            if not target_char:
                print("❌ 未找到目标特征值")
                return False
            
            # 订阅通知
            if "notify" in target_char.properties:
                print("🔔 订阅通知...")
                await self.client.start_notify(target_char.uuid, self.notification_handler)
                print("✅ 已订阅通知，等待数据...")
                
                # 监听数据（持续60秒）
                print("\n⏳ 监听数据中（60秒后自动停止）...")
                await asyncio.sleep(60)
                
                await self.client.stop_notify(target_char.uuid)
                print("🛑 停止监听")
            else:
                print("❌ 特征值不支持notify")
                return False
            
            return True
            
        except Exception as e:
            print(f"❌ 连接/通信错误: {e}")
            return False
        finally:
            if self.client and self.connected:
                await self.client.disconnect()
                print("🔌 已断开连接")
    
    async def run_test(self):
        """运行完整测试"""
        print("=" * 50)
        print("糖尿病初筛腕带 - BLE通信测试")
        print("=" * 50)
        
        # 扫描设备
        devices = await self.scan_devices()
        
        if not devices:
            print("❌ 未找到目标设备")
            print("请确保:")
            print("1. 腕带已上电")
            print("2. BLE已启用")
            print("3. 设备名称为 'DiabetesSensor'")
            return False
        
        # 连接第一个找到的设备
        device = devices[0]
        
        # 连接并监听
        success = await self.connect_and_listen(device)
        
        # 显示统计信息
        if self.received_data:
            print(f"\n📊 测试统计:")
            print(f"   收到数据包数: {len(self.received_data)}")
            
            if len(self.received_data) > 0:
                avg_interval = 60.0 / len(self.received_data) if len(self.received_data) > 0 else 0
                print(f"   平均间隔: {avg_interval:.1f} 秒/包")
                
                # 显示最后一个数据包
                last_data = self.received_data[-1]
                print(f"\n📋 最后一个数据包:")
                print(f"   {json.dumps(last_data, indent=2, ensure_ascii=False)}")
        
        return success

def main():
    """主函数"""
    print("Python BLE测试脚本")
    print("依赖: bleak, asyncio")
    print("安装: pip install bleak")
    print()
    
    tester = BLETester()
    
    try:
        # 运行测试
        success = asyncio.run(tester.run_test())
        
        if success:
            print("\n✅ 测试完成")
            sys.exit(0)
        else:
            print("\n❌ 测试失败")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print("\n\n🛑 用户中断")
        sys.exit(0)
    except Exception as e:
        print(f"\n❌ 测试异常: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()