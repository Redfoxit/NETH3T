FROM python:3.12-slim
# tzdata so the daily word rolls over at local midnight rather than UTC midnight
RUN pip install --no-cache-dir tzdata
WORKDIR /app
COPY server.py /app/server.py
COPY data/answers.txt data/guesses.txt data/params.json /app/wordlists/
# results.jsonl (game history) is written here; mount a volume to keep it across upgrades
ENV DATA_DIR=/data PORT=8088 TZ_NAME=Australia/Melbourne
RUN mkdir -p /data
EXPOSE 8088
CMD ["sh", "-c", "cp -f /app/wordlists/* /data/ && exec python /app/server.py"]
