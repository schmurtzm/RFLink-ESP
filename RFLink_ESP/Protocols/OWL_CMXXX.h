

unsigned short int power(const unsigned char *d)
{
  unsigned short int val = 0;
  val = (d[4] << 8) + d[3];
  val = val & 0xFFF0;
  val = val * 1.00188;
  return val;
}

unsigned long long total(const unsigned char *d)
{
  unsigned long long val = 0;
  if ((d[1] & 0x0F) == 0)
  {
    // Sensor returns total only if nibble#4 == 0
    val = (unsigned long long)d[10] << 40;
    val += (unsigned long long)d[9] << 32;
    val += (unsigned long)d[8] << 24;
    val += (unsigned long)d[7] << 16;
    val += d[6] << 8;
    val += d[5];
  }
  return val;
}

static int validate_os_checksum(r_device *decoder, unsigned char *msg, int checksum_nibble_idx)
{
  // Oregon Scientific v2.1 and v3 checksum is a  1 byte  'sum of nibbles' checksum.
  // with the 2 nibbles of the checksum byte  swapped.
  int i;
  unsigned int checksum, sum_of_nibbles = 0;
  for (i = 0; i < (checksum_nibble_idx - 1); i += 2)
  {
    unsigned char val = msg[i >> 1];
    sum_of_nibbles += ((val >> 4) + (val & 0x0f));
  }

  if (checksum_nibble_idx & 1)
  {
    sum_of_nibbles += (msg[checksum_nibble_idx >> 1] >> 4);
    checksum = (msg[checksum_nibble_idx >> 1] & 0x0f) | (msg[(checksum_nibble_idx + 1) >> 1] & 0xf0);
  }
  else
  {
    checksum = (msg[checksum_nibble_idx >> 1] >> 4) | ((msg[checksum_nibble_idx >> 1] & 0x0f) << 4);
  }

  sum_of_nibbles &= 0xff;

  if (sum_of_nibbles == checksum)
  {
    return 0;
  }
  else
  {
    if (decoder->verbose)
    {
      fprintf(stderr, "Checksum error in Oregon Scientific message.  Expected: %02x  Calculated: %02x\n", checksum, sum_of_nibbles);
      fprintf(stderr, "Message: ");
      bitrow_print(msg, ((checksum_nibble_idx + 4) >> 1) * 8);
    }
    return 1;
  }
}

if ((msg[0] == 0x20) || (msg[0] == 0x21) || (msg[0] == 0x22) || (msg[0] == 0x23) || (msg[0] == 0x24))
{
  //  Owl CM160 Readings
  msg[0] = msg[0] & 0x0f;
  if (validate_os_checksum(decoder, msg, 22) == 0)
  {
    float rawAmp = (msg[4] >> 4 << 8 | (msg[3] & 0x0f) << 4 | msg[3] >> 4);
    unsigned short int ipower = (rawAmp / (0.27 * 230) * 1000);
    data = data_make(
        "brand", "", DATA_STRING, "OS",
        "model", "", DATA_STRING, "CM160",
        "id", "House Code", DATA_INT, msg[1] & 0x0F,
        "power_W", "Power", DATA_FORMAT, "%d W", DATA_INT, ipower,
        NULL);
    decoder_output_data(decoder, data);
  }
}
else if (msg[0] == 0x26)
{
  //  Owl CM180 readings
  msg[0] = msg[0] & 0x0f;
  int valid = validate_os_checksum(decoder, msg, 23);
  int k;
  for (k = 0; k < BITBUF_COLS; k++)
  { // Reverse nibbles
    msg[k] = (msg[k] & 0xF0) >> 4 | (msg[k] & 0x0F) << 4;
  }

  unsigned short int ipower = power(msg);
  unsigned long long itotal = total(msg);
  float total_energy = itotal / 3600 / 1000.0;
  if (itotal && valid == 0)
  {
    data = data_make(
        "brand", "", DATA_STRING, "OS",
        "model", "", DATA_STRING, "CM180",
        "id", "House Code", DATA_INT, msg[1] & 0x0F,
        "power_W", "Power", DATA_FORMAT, "%d W", DATA_INT, ipower,
        "energy_kWh", "Energy", DATA_FORMAT, "%2.1f kWh", DATA_DOUBLE, total_energy,
        NULL);
    decoder_output_data(decoder, data);
  }
  else if (!itotal)
  {
    data = data_make(
        "brand", "", DATA_STRING, "OS",
        "model", "", DATA_STRING, "CM180",
        "id", "House Code", DATA_INT, msg[1] & 0x0F,
        "power_W", "Power", DATA_FORMAT, "%d W", DATA_INT, ipower,
        NULL);
    decoder_output_data(decoder, data);
  }
}