﻿// Copyright 2020 Maris Melbardis
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace EmuController.Client.NET.Input
{
    public class Buttons
    {
        internal readonly UsageId Usage = UsageId.Button;

        internal readonly BitArray ArrayMap;
        internal readonly BitArray ButtonArray;

        internal ushort[] ButtonValues { get; private set; }


        internal Buttons()
        {
            ArrayMap = new BitArray(8);
            ButtonArray = new BitArray(128);
            ButtonValues = new ushort[8];
        }


        /// <summary>
        /// Set state for button with specified index.
        /// </summary>
        /// <param name="index"></param>
        /// <param name="value"></param>
        public void SetButton(int index, bool value)
        {
            if (index < -0 || index > 127)
            {
                throw new ArgumentOutOfRangeException(nameof(index));
            }
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
            Buffer.BlockCopy(temp, 0, ButtonValues, 0, temp.Length);

        }
    }
}
