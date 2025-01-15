#include <criterion/criterion.h> // Inclui a biblioteca Criterion

// Função para verificar se a temperatura está dentro do intervalo razoável
int reasonable_temperature(int16_t temperature) {
    if (temperature > 200 && temperature < 300) { // Entre 20.0°C e 30.0°C
        return 1;
    } else {
        return 0;
    }
}

// Função para verificar se a umidade está dentro do intervalo razoável
int reasonable_humidity(int16_t humidity) {
    if (humidity >= 300 && humidity <= 800) { // Entre 30.0% e 80.0%
        return 1;
    } else {
        return 0;
    }
}

Test(sensor_values, test_temperature) {
    // Testa valores de temperatura
    cr_assert(reasonable_temperature(250) == 1, "Temperatura 25.0°C deveria ser considerada razoável");
}

Test(sensor_values, test_humidity) {
    // Testa valores de umidade
    cr_assert(reasonable_humidity(500) == 1, "Umidade 50.0%% deveria ser considerada razoável");
}
