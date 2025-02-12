
    async function fetchTemperatureData() {
        const response = await fetch(
            'https://sheets.googleapis.com/v4/spreadsheets/B8b9ZLW67eSutCG2f_y0Ar1PL_Xo2spWAiFROM/values/ESR!A2:C?key=AIzaSyAoApzYIbKdNxPmVKwihgcnzP-xjAQXS4c'
        );
        const data = await response.json();
        const rows = data.values;

        const labels = rows.map(row => `${row[0]} ${row[1]}`);  // Date and Time
        const temperatures = rows.map(row => parseFloat(row[2]));  // Temperature column

        return { labels, temperatures };
    }

    async function renderChart() {
        const ctx = document.getElementById('temperatureChart').getContext('2d');
        const { labels, temperatures } = await fetchTemperatureData();

        new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Temperature (°C)',
                    data: temperatures,
                    borderColor: 'rgba(75, 192, 192, 1)',
                    backgroundColor: 'rgba(75, 192, 192, 0.2)',
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: false
                    }
                }
            }
        });
    }

    document.addEventListener('DOMContentLoaded', renderChart);
