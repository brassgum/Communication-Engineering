#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

typedef struct
{
	double real;
	double imag;
} complex_t;

#pragma pack(push, 1)

typedef struct format_header_s
{
	unsigned int block_size;
	unsigned short audio_format;
	unsigned short channels;
	unsigned int sample_rate;
	unsigned int byte_per_sec;
	unsigned short byte_per_block;
	unsigned short bits_per_sample;
} format_header_t;

#pragma pack(pop)

typedef struct wav_data_s
{
	unsigned int size;
	short* data;
} wav_data_t;

typedef enum sideband
{
	SIDEBAND_USB = 0,
	SIDEBAND_LSB = 1
} sideband_t;

typedef struct am_carrier_s
{
	double amplitude;
	double frequency;
	double phase;
} am_carrier_t;

wav_data_t load_sample(format_header_t* out_header);
void release_wav_data(wav_data_t* data);
complex_t complex_add(const complex_t* a, const complex_t* b);
complex_t complex_sub(const complex_t* a, const complex_t* b);
complex_t complex_mul(const complex_t* a, const complex_t* b);
complex_t complex_exp(double theta);
double complex_mag(const complex_t* a);
int* br_order_malloc(int bit_size);
complex_t* fft_malloc(double* input, size_t samples, int* out_size);
complex_t* ifft_malloc(const complex_t* freq_domain, int n);
double* am_dsb_lc_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs);
double* am_dsb_sc_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs);
double* am_ssb_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs, sideband_t sideband);

int main(void)
{
	const double PI = acos(-1);

	int fc = 100000; // 100kHz carrier frequency
	double ac = 1.0;

	format_header_t header;
	wav_data_t sample = load_sample(&header);

	printf("block_size: %d, audio_format: %d, channels: %d,\n"
		"sample_rate: %d, byte per second: %d, byte per block: %d\n"
		"bits per sample: %d\n", header.block_size, header.audio_format, header.channels
		, header.sample_rate, header.byte_per_sec, header.byte_per_block
		, header.bits_per_sample);

	printf("data size: %d\n", sample.size);

	double* input = malloc(sizeof(double) * sample.size);
	for (size_t i = 0; i < sample.size; ++i)
	{
		input[i] = (double)sample.data[i] / (double)0x7fff;
	}


	release_wav_data(&sample);
	free(input);
	return 0;
}


wav_data_t load_sample(format_header_t* out_header)
{
	FILE* sample = fopen("resources/sample.wav", "rb");
	assert(sample != NULL);
	wav_data_t res;
	fseek(sample, 16, SEEK_SET);
	fread(out_header, sizeof(format_header_t), 1, sample);

	fseek(sample, out_header->block_size + 24, SEEK_SET);
	unsigned int data_chunk_size;
	fread(&data_chunk_size, sizeof(unsigned int), 1, sample);
	res.data = malloc(data_chunk_size);
	if (res.data == NULL)
	{
		goto lb_malloc_fail;
	}
	res.size = data_chunk_size / sizeof(short);
	fread(res.data, sizeof(short), res.size, sample);


	fclose(sample);
	return res;

lb_malloc_fail:
	fclose(sample);
	assert(res.data != NULL);
}

void release_wav_data(wav_data_t* data)
{
	free(data->data);
}

complex_t complex_add(const complex_t* a, const complex_t* b)
{
	complex_t res;
	res.real = a->real + b->real;
	res.imag = a->imag + b->imag;
	return res;
}

complex_t complex_sub(const complex_t* a, const complex_t* b)
{
	complex_t res;
	res.real = a->real - b->real;
	res.imag = a->imag - b->imag;
	return res;
}

complex_t complex_mul(const complex_t* a, const complex_t* b)
{
	complex_t res;
	res.real = a->real * b->real - a->imag * b->imag;
	res.imag = a->real * b->imag + b->real * a->imag;
	return res;
}

complex_t complex_exp(double theta)
{
	complex_t res;
	res.real = cos(theta);
	res.imag = sin(theta);
	return res;
}

double complex_mag(const complex_t* a)
{
	return sqrt(a->real * a->real + a->imag * a->imag);
}

int* br_order_malloc(int bit_size)
{
	int size = 1 << bit_size;
	int* res = malloc(sizeof(int) * size);
	assert(res != NULL);

	int bit;
	int temp;

	for (int i = 0; i < size; ++i)
	{
		temp = 0;
		for (int j = 0; j < bit_size; ++j)
		{
			bit = (i >> j) & 1;
			temp = (temp << 1) | bit;
		}
		res[i] = temp;
	}

	return res;
}

complex_t* fft_malloc(double* input, size_t samples, int* out_size)
{
	const double PI = acos(-1);
	const complex_t RE1 = { 1.0, 0.0 };
	// br_order copy
	int bit_size = (int)ceil(log2(samples));
	int size = 1 << bit_size;
	complex_t* res = malloc(sizeof(complex_t) * size);
	assert(res != NULL);

	int* br_order = br_order_malloc(bit_size);
	int idx;
	int i;
	int j;
	int k;
	*out_size = size;

	for (i = 0; i < size; ++i)
	{
		idx = br_order[i];
		if (idx < samples)
		{
			res[i].real = input[idx];
			res[i].imag = 0.0;
		}
		else
		{
			res[i].real = 0.0;
			res[i].imag = 0.0;
		}
	}

	for (i = 0; i < bit_size; ++i)
	{
		int m = 2 << i;
		int m_half = m >> 1;
		complex_t wm = complex_exp(-2 * PI / (double)m);
		for (j = 0; j < size; j += m)
		{
			complex_t w = RE1;
			for (k = 0; k < m_half; ++k)
			{
				complex_t temp0 = complex_mul(&w, res + j + k + m_half);
				complex_t temp1 = res[j + k];
				res[j + k] = complex_add(&temp1, &temp0);
				res[j + k + m_half] = complex_sub(&temp1, &temp0);
				w = complex_mul(&w, &wm);
			}
		}
	}

	free(br_order);

	return res;
}

complex_t* ifft_malloc(const complex_t* freq_domain, int n)
{
	int bit_size = (int)ceil(log2(n));
	int length = 1 << bit_size;
	complex_t* res = malloc(sizeof(complex_t) * length);
	assert(res != NULL);

	int* br_order = br_order_malloc(bit_size);
	int i;
	int j;
	int k;
	int idx;
	int m;
	int m_half;
	complex_t wm;
	complex_t w;
	const complex_t RE1 = { 1.0, 0.0 };
	const double PI = acos(-1);
	complex_t temp0;
	complex_t temp1;


	for (i = 0; i < length; ++i)
	{
		idx = br_order[i];
		res[i].real = freq_domain[idx].real;
		res[i].imag = freq_domain[idx].imag;
	}

	m = 2;
	while (m <= n)
	{
		m_half = m >> 1;
		wm = complex_exp(2 * PI / m);

		for (i = 0; i < n; i += m)
		{
			w = RE1;

			for (j = 0; j < m_half; ++j)
			{
				temp0 = complex_mul(&w, res + i + j + m_half);
				temp1 = res[i + j];

				res[i + j] = complex_add(&temp1, &temp0);
				res[i + j + m_half] = complex_sub(&temp1, &temp0);

				w = complex_mul(&w, &wm);
			}
		}
		m = m << 1;
	}

	for (i = 0; i < length; ++i)
	{
		res[i].real = res[i].real / n;
		res[i].imag = res[i].imag / n;
	}

	free(br_order);

	return res;
}

double* am_dsb_lc_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs)
{

	return NULL;
}

double* am_dsb_sc_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs)
{
	return NULL;
}

double* am_ssb_malloc(const double* input, size_t samples, const am_carrier_t* carrier, double fs, sideband_t sideband)
{
	return NULL;
}
