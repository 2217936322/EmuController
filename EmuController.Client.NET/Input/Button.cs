﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EmuController.Client.NET.Input
{
    public class Button
    {
        internal readonly UsageId Usage = UsageId.Button;

        internal readonly BitArray ArrayMap;
        internal readonly BitArray ButtonArray;

        internal ushort[] Buttons { get; private set; }


        internal Button()
        {
            ArrayMap = new BitArray(8);
            ButtonArray = new BitArray(128);
            Buttons = new ushort[8];
        }


        public void SetButton(int index, bool value)
        {
            int arrayMapIndex = (index / 16);
            if (arrayMapIndex < 0)
            {
                arrayMapIndex = 0;
            }
            ArrayMap.Set(arrayMapIndex, true);
            ButtonArray.Set(index, value);
        }

        internal void GetButtons()
        {
            byte[] temp = new byte[16];
            ButtonArray.CopyTo(temp, 0);
            Buffer.BlockCopy(temp, 0, Buttons, 0, temp.Length);

        }
    }
}