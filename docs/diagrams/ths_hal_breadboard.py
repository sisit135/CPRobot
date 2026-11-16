#!/usr/bin/env python3
"""Render the THS v2 HAL electrical sketch; modules use logical connector order."""
from html import escape
from pathlib import Path
import argparse
import re

OUT = Path(__file__).with_suffix('.svg')
C = {'3V3':'#dc2626', 'GND':'#374151', 'MAIN SDA':'#0891b2',
     'MAIN SCL':'#2563eb', 'TOF SDA':'#16a34a', 'TOF SCL':'#65a30d',
     'XL':'#c026d3', 'XF':'#9333ea', 'XR':'#db2777',
     'AIN1':'#b45309', 'AIN2':'#ea580c', 'BIN1':'#7c3aed', 'BIN2':'#4f46e5',
     'nSLEEP':'#0f766e', 'nFAULT':'#a21caf', 'SCK':'#ca8a04',
     'MOSI':'#e11d48', 'MISO':'#0284c7', 'CS':'#7c2d12', 'READY':'#059669',
     'KEY':'#475569', 'VM':'#991b1b', 'LEFT+':'#92400e', 'LEFT-':'#d97706',
     'RIGHT+':'#4338ca', 'RIGHT-':'#818cf8'}

def validate_sources():
    """Reject stale drawings and duplicate default GPIO allocations without SDK tools."""
    root = Path(__file__).resolve().parents[2]
    fw = root / 'fw/autonomous/STM32_TINYRCP'
    motors = (fw / 'include/board_motor_pins.hpp').read_text()
    sensors = (fw / 'include/hal/sensors_config.hpp').read_text()
    profiles = (fw / 'platformio.ini').read_text()
    assignments = {}
    expected_motors = {'ain1_gpio':'PA8', 'ain2_gpio':'PA9', 'bin1_gpio':'PB8',
                       'bin2_gpio':'PB9', 'nsleep_gpio':'PB12', 'nfault_gpio':'PB13'}
    expected_sensors = {'MAIN_SDA':'PB7', 'MAIN_SCL':'PB6', 'TOF_SDA':'PB3',
                        'TOF_SCL':'PB10', 'TOF_LEFT_XSHUT':'PA1',
                        'TOF_FRONT_XSHUT':'PA2', 'TOF_RIGHT_XSHUT':'PA3'}
    expected_flags = {'SPI_CS':'PA4', 'SPI_READY':'PB0', 'LOC_KEY':'PB1'}
    for name, pin in expected_motors.items():
        found = re.search(r'\b'+name+r'\s*=\s*(P[A-Z]\d+)\b', motors)
        if not found or found[1] != pin:
            raise ValueError(f'Stale motor drawing: {name} must be {pin}')
        assignments[name] = pin
    for name, pin in expected_sensors.items():
        found = re.search(r'^#define SENSORS_'+name+r'_PIN\s+(P[A-Z]\d+)\b', sensors, re.M)
        if not found or found[1] != pin:
            raise ValueError(f'Stale sensor drawing: {name} must be {pin}')
        assignments[name] = pin
    for name, pin in expected_flags.items():
        found = re.findall(r'-DAXI_'+name+r'_PIN=(P[A-Z]\d+)\b', profiles)
        if not found or set(found) != {pin}:
            raise ValueError(f'Stale profile drawing: {name} must be {pin}')
        assignments[name] = pin
    for name, pin in {'SPI1_SCK':'PA5', 'SPI1_MISO':'PA6', 'SPI1_MOSI':'PA7',
                      'USB_DM':'PA11', 'USB_DP':'PA12',
                      'SWDIO':'PA13', 'SWCLK':'PA14'}.items():
        assignments[name] = pin
    if len(set(assignments.values())) != len(assignments):
        raise ValueError(f'Conflicting GPIO assignments: {assignments}')
    return len(assignments)

def render():
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="2400" height="1700" viewBox="0 0 2400 1700" role="img" aria-labelledby="title desc">',
         '<title id="title">THS v2 HAL wiring with a 200-contact breadboard</title>',
         '<desc id="desc">Firmware-assigned electrical wiring. Module connectors are logical power, signal, miscellaneous order, not physical pin order. Straight wire vectors have no splices; named bus terminals connect through the breadboard. Two isolated I2C controllers avoid the TCS and ToF reset address conflict.</desc>',
         '<style>text{font-family:DejaVu Sans,Arial,sans-serif;fill:#172033} .small{font-size:16px} .pin{font-size:18px} .head{font-size:24px;font-weight:bold} .note{font-size:19px} .board{fill:#f1f5f9;stroke:#64748b;stroke-width:2} .wire{fill:none;stroke-width:3;stroke-linecap:round} .terminal{fill:white;stroke-width:2}</style>',
         '<rect width="2400" height="1700" fill="#ffffff"/>']
    def text(x,y,t,cl='note',anchor='start'):
        s.append(f'<text x="{x}" y="{y}" class="{cl}" text-anchor="{anchor}">{escape(t)}</text>')
    def rect(x,y,w,h,fill='#f1f5f9',stroke='#64748b',rx=12):
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
    def wire(a,b,net,label=None):
        color=C[net]
        s.append(f'<line class="wire" x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="{color}"><title>{escape(label or net)}</title></line>')
        for x,y in [a,b]:
            s.append(f'<circle cx="{x}" cy="{y}" r="4" fill="white" stroke="{color}" stroke-width="2"/>')
    def tag(x,y,net,side='right',label=None):
        # Open-circle terminal: a labelled bus endpoint, never a wire splice.
        text(x + (9 if side=='right' else -9), y-6, label or net,'small','start' if side=='right' else 'end')
        s.append(f'<circle cx="{x}" cy="{y}" r="5" class="terminal" stroke="{C[net]}"/>')
    def module(x,y,w,title,subtitle,pins,edge='right'):
        h=95+32*len(pins)
        rect(x,y,w,h)
        text(x+18,y+30,title,'head'); text(x+18,y+57,subtitle,'small')
        result={}
        for i,(name,net) in enumerate(pins):
            yy=y+90+32*i; xx=x+w if edge=='right' else x
            text(x+18 if edge=='right' else x+w-18,yy+6,name,'pin','start' if edge=='right' else 'end')
            s.append(f'<circle cx="{xx}" cy="{yy}" r="5" fill="white" stroke="#64748b" stroke-width="2"/>')
            result[name]=(xx,yy)
            if net:
                end=(xx+(75 if edge=='right' else -75),yy)
                wire((xx,yy),end,net)
                tag(*end,net,side=edge)
        return result
    text(55,55,'THS v2 • HAL electrical wiring','head')
    text(55,90,'STM32F411CE Black Pill • 2 isolated I²C buses • DRV8833 • ESP32-C6 / LuatOS CORE ESP32-C3')
    text(55,125,'Logical connector order: POWER → SIGNALS → MISC. The symbols do not specify physical module pin positions.')
    text(55,158,'Every wire is one straight vector. Crossings are unconnected. Named open-circle bus terminals are connected by the breadboard rows below.')

    # Three independent ToF shutdown nets, common isolated I2C2 bus.
    tofs=[]
    for i,(name,address,net,y) in enumerate([('LEFT','0x30','XL',220),('FRONT','0x31','XF',530),('RIGHT','0x32','XR',840)]):
        p=module(55,y,370,f'VL53L0X • {name}',f'7-bit: boot 0x29 → runtime {address}',
                 [('VCC / VIN','3V3'),('GND','GND'),('SDA','TOF SDA'),('SCL','TOF SCL'),('XSHUT',None)])
        tofs.append((p['XSHUT'],net))
        text(75,y+275,'GPIO1: NC • XSHUT: local logic pull-up','small')

    imu=module(1920,220,420,'MPU9250 + AK8963','7-bit: MPU 0x68 • magnetometer 0x0C',
       [('VCC / VIN','3V3'),('GND','GND'),('SDA','MAIN SDA'),('SCL','MAIN SCL'),('CS: strap to local VDDIO',None),('AD0: strap to local GND',None)],edge='left')
    text(1940,535,'INT / FSYNC / auxiliary I²C: unused','small')
    tcs=module(1920,600,420,'TCS34725','7-bit: fixed 0x29 • separate from ToF boot bus',
       [('VCC / VIN','3V3'),('GND','GND'),('SDA','MAIN SDA'),('SCL','MAIN SCL')],edge='left')
    text(1940,855,'INT: NC • LED: module-local configuration','small')
    bridge=module(1880,940,460,'ESP32 bridge • choose one board','C6 or LuatOS CORE C3 • SPI slave',
       [('3V3 supply / regulator','3V3'),('GND','GND'),('GPIO5 / SCK',None),('GPIO7 / MOSI',None),('GPIO6 / MISO',None),('GPIO4 / CS',None),('GPIO3 / READY',None)],edge='left')
    text(1900,1290,'3V3 needs a separately rated regulator.','small')
    text(1900,1315,'Do not power the radio from Black Pill 3V3.','small')

    module(1490,500,300,'External supplies','Regulated logic + motor rail',
       [('3V3 output','3V3'),('Common return','GND'),('Motor VM output','VM')])

    # Logical MCU pin order, deliberately not the mechanical header order.
    rect(970,260,430,1045,'#e2e8f0','#334155')
    text(1185,298,'BLACK PILL • F411CE','head','middle')
    text(1185,327,'POWER → SIGNALS → MISC','small','middle')
    left=[('3V3 bus','3V3'),('GND bus','GND'),('PB7 / I2C1 SDA','MAIN SDA'),('PB6 / I2C1 SCL','MAIN SCL'),('PB3 / I2C2 SDA','TOF SDA'),('PB10 / I2C2 SCL','TOF SCL'),('PA1 / LEFT XSHUT','XL'),('PA2 / FRONT XSHUT','XF'),('PA3 / RIGHT XSHUT','XR'),('PA8 / TIM1 CH1','AIN1'),('PA9 / TIM1 CH2','AIN2'),('PB8 / TIM4 CH3','BIN1'),('PB9 / TIM4 CH4','BIN2'),('PB12 / nSLEEP','nSLEEP'),('PB13 / nFAULT','nFAULT')]
    mcu={}
    for i,(label,net) in enumerate(left):
        yy=375+52*i; mcu[net]=(970,yy)
        text(990,yy+6,label,'pin')
        if i<6:
            wire((970,yy),(875,yy),net);tag(875,yy,net,'left')
    for i,(label,net) in enumerate([('PA5 / SPI1 SCK','SCK'),('PA7 / SPI1 MOSI','MOSI'),('PA6 / SPI1 MISO','MISO'),('PA4 / SPI1 CS','CS'),('PB0 / READY','READY')]):
        yy=815+62*i;mcu[net]=(1400,yy);text(1380,yy+6,label,'pin','end')
    text(1185,1195,'PA11 / PA12 → USB connector','small','middle')
    text(1185,1225,'PA13 / PA14 → SWD (RTT)','small','middle')
    text(1185,1255,'PA0: on-board KEY • PC13: LED','small','middle')
    for p,net in tofs: wire(p,mcu[net],net)
    for name,net in [('GPIO5 / SCK','SCK'),('GPIO7 / MOSI','MOSI'),('GPIO6 / MISO','MISO'),('GPIO4 / CS','CS'),('GPIO3 / READY','READY')]: wire(mcu[net],bridge[name],net)
    # Key is physically separate from the on-board PA0 button.
    text(1470,320,'Localization key','head');text(1470,350,'External normally-open switch','small')
    text(1420,405,'PB1 / internal pull-up','pin')
    wire((1400,420),(1630,420),'KEY')
    s.append('<circle cx="1630" cy="420" r="5" fill="white" stroke="#475569" stroke-width="2"/><circle cx="1700" cy="420" r="5" fill="white" stroke="#475569" stroke-width="2"/><line x1="1630" y1="420" x2="1695" y2="395" stroke="#475569" stroke-width="3"/>')
    wire((1700,420),(1760,420),'GND');tag(1760,420,'GND')

    drv=module(55,1190,420,'DRV8833 • 2 bridges','A → left motor • B → right motor',
       [('VM: motor supply','VM'),('GND','GND'),('AIN1',None),('AIN2',None),('BIN1',None),('BIN2',None),('nSLEEP',None),('nFAULT',None)])
    for n in ['AIN1','AIN2','BIN1','BIN2','nSLEEP','nFAULT']: wire(drv[n],mcu[n],n)
    text(550,1570,'AOUT1 / AOUT2 → LEFT motor','pin')
    text(550,1602,'BOUT1 / BOUT2 → RIGHT motor','pin')
    for xx,label in [(110,'AO1'),(210,'AO2'),(310,'BO1'),(410,'BO2')]: text(xx,1530,label,'small','middle')
    wire((110,1541),(540,1570),'LEFT+');wire((210,1541),(540,1583),'LEFT-')
    wire((310,1541),(540,1602),'RIGHT+');wire((410,1541),(540,1615),'RIGHT-')
    text(75,1620,'nFAULT: 10 kΩ external pull-up to 3V3','small')
    text(75,1648,'VM: 2.7–10.8 V, selected for motors','small')

    # 200 contacts exactly: 20 rows × (5 + 5), each half-row connected internally.
    bx,by=1030,1380; pitch=10; step=12
    rect(bx-20,by-32,460,280,'#fffdf5','#cbd5e1')
    text(bx-10,by-46,'200-contact breadboard • 20 × (5 + 5)','head')
    for row in range(20):
        yy=by+row*step
        text(bx-25,yy+4,str(row+1),'small','end')
        for half in range(2):
            xx=bx+half*280
            s.append(f'<line x1="{xx}" y1="{yy}" x2="{xx+40}" y2="{yy}" stroke="#cbd5e1" stroke-width="3"/>')
            for col in range(5):
                s.append(f'<circle cx="{xx+col*pitch}" cy="{yy}" r="3" fill="#64748b"/>')
        if row<6:
            lname=['3V3','GND','MAIN SDA','MAIN SCL','3V3','GND'][row]
            rname=['3V3','GND','TOF SDA','TOF SCL','3V3','GND'][row]
            text(bx+52,yy+5,lname,'small');text(bx+332,yy+5,rname,'small')
    # Four half-rows per supply: 20 contacts minus 6 jumper ends = 14 taps.
    for row,net in [(0,'3V3'),(1,'GND')]:
        wire((bx+30,by+row*step),(bx+30,by+(row+4)*step),net)
        wire((bx+280+30,by+row*step),(bx+280+30,by+(row+4)*step),net)
        wire((bx+40,by+(row+4)*step),(bx+280,by+(row+4)*step),net)
    text(bx+160,by+105,'center gap','small','middle')
    text(bx+160,by+135,'rows 7–20 spare','small','middle')
    text(bx+160,by+165,'Same names = same bus.','small','middle')
    text(bx+160,by+195,'14 taps per supply rail.','small','middle')
    # Power input symbols make supplies explicit without claiming a battery design.
    text(1515,1400,'POWER / BUS NOTES','head')
    for i,line in enumerate([
        '3V3 = regulated logic bus; all GNDs common.',
        'Use an adequately rated external 3V3 regulator.',
        'Black Pill 3V3 pin is supply input in this sketch.',
        'Motor VM stays off the breadboard logic buses.',
        'USB: data + GND; avoid feeding a second power source.',
        '4 × I²C pull-ups: SDA/SCL → 3V3 (e.g. 4.7 kΩ).',
        'XSHUT: open-drain; pull up to module-permitted rail.',
        'Select sensor breakouts accepting 3V3 supply / I²C.',
        'Optional on-board SPI flash must be absent/isolated.',
        'PB3 = I²C2; SWO unavailable. SWD RTT is retained.'
    ]): text(1515,1432+i*23,line,'small')
    text(55,1680,'SOURCE-CONFIGURED DESIGN • not a physical qualification • USB upload/serial and SWD use existing board connectors • no encoder wiring')
    s.append('</svg>')
    return '\n'.join(s)+'\n'

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    count=validate_sources()
    print(f"GPIO audit: {count} active/reserved default pins are distinct")
    data=render()
    if args.check:
        raise SystemExit(0 if OUT.exists() and OUT.read_text()==data else 'SVG missing or stale')
    OUT.write_text(data)
    print(OUT)
