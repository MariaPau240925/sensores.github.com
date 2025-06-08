 Nombre del archivo CSV
filename = 'query (5).csv'; % Cambia esto por el nombre de tu archivo

% Leer el archivo CSV
data = readtable(filename);

disp('Nombres de las columnas:');
disp(data.Properties.VariableNames);

value = data.Var5; 

if iscell(value)
    value = cell2mat(value); 
end

% Número de muestras
N = length(value);

dt = 0.1; % Intervalo de muestreo en segundos
fs = 1/dt;
fprintf('Frecuencia de muestreo asumida: %.3f Hz\n', fs);

%Validar criterio de Nyquist
disp('Asumiendo muestreo uniforme y frecuencia máxima menor que Nyquist.');

%Calcular DFT manualmente
X = zeros(N, 1); %vector de salida
for k = 0:N-1
    suma = 0;
    for n = 0:N-1
        suma = suma + value(n+1) * exp(-1j * (2 * pi / N) * k * n);
    end
    X(k+1) = suma;
end

% Calcular la magnitud
P2 = abs(X/N); % Magnitud de la DFT
P1 = P2(1:floor(N/2)+1); 
P1(2:end-1) = 2 * P1(2:end-1); 
f = fs * (0:floor(N/2)) / N; % Vector de frecuencias

% Aplicar filtro digital pasa bajos
fc = 0.1; % Frecuencia de corte Hz
Wn = 2 * pi * fc / fs; 
alpha = exp(-Wn); % Coeficiente del filtro

%señal filtrada
filtered_value = zeros(size(value));

% Aplicar el filtro 
for n = 2:N
    filtered_value(n) = (1 - alpha) * value(n) + alpha * filtered_value(n-1);
end

%Calcular SNR antes y después del filtro
signal_power = mean(value.^2);
noise_power = mean((value - filtered_value).^2);
snr_antes= 10 * log10(signal_power / noise_power);
snr_despues= 10 * log10(signal_power / noise_power);

fprintf('SNR antes del filtrado: %.2f dB\n', snr_antes);
fprintf('SNR después del filtrado: %.2f dB\n', snr_despues);

% Paso 5: Graficar resultados
figure;
subplot(3, 1, 1);
plot((0:N-1) *  value);
title('Señal Original');
xlabel('Tiempo (s)');
ylabel('Valor');
grid on;

subplot(3, 1, 2);
plot((0:N-1) * dt, filtered_value);
title('Señal Filtrada (Filtro Manual)');
xlabel('Tiempo (s)');
ylabel('Valor Filtrado');
grid on;

subplot(3, 1, 3);
plot(f, P1);
title('Espectro de Frecuencia de la Señal Original (DFT)');
xlabel('Frecuencia (Hz)');
ylabel('|P1(f)|');
xlim([0 fs/2]);
grid on;