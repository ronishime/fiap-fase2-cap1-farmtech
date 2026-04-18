"""
================================================================================
FarmTech Solutions - Fase 2 Cap 1
Consulta Climatica para Sistema de Irrigacao - Cultura: Soja
================================================================================
Versao: v1
Autor: Ronaldo Nishime

Descricao:
    Script Python que consulta a API publica Open-Meteo para obter previsao
    de chuva para a localizacao da fazenda (Mogi das Cruzes, SP). Com base na
    previsao das proximas 6 horas, decide se a irrigacao deve ser bloqueada.

    Saida: uma recomendacao textual e uma constante formatada pronta para
    ser copiada manualmente para o codigo do ESP32 (sketch.ino).

API utilizada:
    Open-Meteo Forecast API (https://open-meteo.com)
    - Nao requer cadastro nem API key
    - Gratuita para uso nao-comercial
    - Documentacao: https://open-meteo.com/en/docs

Lógica de decisão:
    - Bloqueia irrigacao se chuva acumulada prevista > 2 mm nas proximas 6h
      OU probabilidade maxima de chuva > 60% no mesmo periodo.
================================================================================
"""

import requests
import sys
from datetime import datetime

# ---------- Configuracoes ----------
LATITUDE = -23.52
LONGITUDE = -46.19
CIDADE = "Mogi das Cruzes, SP"

# Limiares de decisao
LIMIAR_CHUVA_MM = 2.0          # mm de chuva acumulada nas proximas 6h
LIMIAR_PROB_CHUVA = 60         # % de probabilidade maxima
JANELA_HORAS = 6               # horas a frente para avaliar

# URL da API Open-Meteo
URL_API = "https://api.open-meteo.com/v1/forecast"


def consultar_previsao(lat: float, lon: float) -> dict:
    """Faz requisicao a API Open-Meteo e retorna o JSON da resposta."""
    parametros = {
        "latitude": lat,
        "longitude": lon,
        "hourly": "precipitation,precipitation_probability",
        "timezone": "America/Sao_Paulo",
        "forecast_days": 1,
    }

    try:
        resposta = requests.get(URL_API, params=parametros, timeout=10)
        resposta.raise_for_status()
        return resposta.json()
    except requests.exceptions.Timeout:
        print("ERRO: timeout ao consultar a API Open-Meteo.")
        sys.exit(1)
    except requests.exceptions.RequestException as e:
        print(f"ERRO ao consultar API: {e}")
        sys.exit(1)


def analisar_janela(dados: dict, janela_horas: int) -> tuple:
    """
    Extrai da previsao as proximas N horas a partir do horario atual.
    Retorna (chuva_acumulada_mm, prob_maxima_pct, linhas_detalhe).
    """
    horarios = dados["hourly"]["time"]
    precipitacao = dados["hourly"]["precipitation"]
    probabilidade = dados["hourly"]["precipitation_probability"]

    agora = datetime.now()
    chuva_total = 0.0
    prob_max = 0
    linhas = []

    horas_analisadas = 0
    for i, ts in enumerate(horarios):
        hora_prev = datetime.fromisoformat(ts)
        if hora_prev < agora.replace(minute=0, second=0, microsecond=0):
            continue
        if horas_analisadas >= janela_horas:
            break

        mm = precipitacao[i] if precipitacao[i] is not None else 0.0
        pct = probabilidade[i] if probabilidade[i] is not None else 0

        chuva_total += mm
        prob_max = max(prob_max, pct)
        linhas.append(f"  {hora_prev.strftime('%H:%M')}  chuva: {mm:5.2f} mm  prob: {pct:3d}%")

        horas_analisadas += 1

    return chuva_total, prob_max, linhas


def decidir_bloqueio(chuva_mm: float, prob_pct: int) -> tuple:
    """Aplica a regra de decisao. Retorna (bloquear_bool, justificativa_str)."""
    if chuva_mm > LIMIAR_CHUVA_MM:
        return True, f"Chuva acumulada prevista ({chuva_mm:.2f} mm) acima do limiar ({LIMIAR_CHUVA_MM} mm)"
    if prob_pct > LIMIAR_PROB_CHUVA:
        return True, f"Probabilidade maxima de chuva ({prob_pct}%) acima do limiar ({LIMIAR_PROB_CHUVA}%)"
    return False, "Previsao de chuva baixa: irrigacao liberada"


def main():
    print("=" * 70)
    print("FarmTech Solutions - Consulta Climatica para Irrigacao")
    print("=" * 70)
    print(f"Localizacao: {CIDADE}")
    print(f"Coordenadas: ({LATITUDE}, {LONGITUDE})")
    print(f"Janela de analise: proximas {JANELA_HORAS} horas")
    print(f"Data/hora da consulta: {datetime.now().strftime('%d/%m/%Y %H:%M:%S')}")
    print()

    # Consulta a API
    print("Consultando Open-Meteo...")
    dados = consultar_previsao(LATITUDE, LONGITUDE)
    print("Dados recebidos com sucesso.")
    print()

    # Analisa janela de previsao
    chuva_total, prob_max, linhas_detalhe = analisar_janela(dados, JANELA_HORAS)

    print("Previsao hora a hora:")
    for linha in linhas_detalhe:
        print(linha)
    print()

    print(f"Chuva acumulada (prox. {JANELA_HORAS}h): {chuva_total:.2f} mm")
    print(f"Probabilidade maxima de chuva:    {prob_max}%")
    print()

    # Decisao
    bloquear, motivo = decidir_bloqueio(chuva_total, prob_max)

    print("-" * 70)
    print("DECISAO:")
    print(f"  Irrigacao: {'BLOQUEADA' if bloquear else 'liberada'}")
    print(f"  Motivo:    {motivo}")
    print("-" * 70)
    print()

    # Saida formatada para copiar no sketch.ino
    valor_constante = "true" if bloquear else "false"
    print("Copie a linha abaixo para o sketch.ino (secao de constantes):")
    print()
    print(f"  #define CHUVA_PREVISTA {valor_constante}   // Atualizado em {datetime.now().strftime('%d/%m/%Y %H:%M')}")
    print()


if __name__ == "__main__":
    main()